// Shell.

// #include <stdlib.h> // Required for malloc and free
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAXARGS 10

struct cmd
{
  int type;
};

struct execcmd
{
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd
{
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd
{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd
{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd
{
  int type;
  struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit();

  switch (cmd->type)
  {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit();
    exec(ecmd->argv[0], ecmd->argv);
    printf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0)
    {
      printf(2, "open %s failed\n", rcmd->file);
      exit();
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
    wait();
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    if (pipe(p) < 0)
      panic("pipe");
    if (fork1() == 0)
    {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0)
    {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait();
    wait();
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit();
}

int x = 0;

char *
fmtname(char *path)
{
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  // The original function lacks a null terminator; we handle this when copying.
  return buf;
}

char **
get_all_commands(int *command_count)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  char **command_list;
  int count = 0;
  int i;
  char *path = ".";

  if ((fd = open(path, 0)) < 0)
  {
    printf(2, "get_all_commands: cannot open %s\n", path);
    return 0;
  }
  while (read(fd, &de, sizeof(de)) == sizeof(de))
  {
    if (de.inum != 0 && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0)
      count++;
  }
  close(fd);

  int total_commands = count + 1;
  command_list = (char **)malloc(total_commands * sizeof(char *));
  if (command_list == 0)
    return 0;

  if ((fd = open(path, 0)) < 0)
  {
    free(command_list);
    return 0;
  }

  i = 0;
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';

  while (read(fd, &de, sizeof(de)) == sizeof(de) && i < count)
  {
    if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;

    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    char *formatted_name = fmtname(buf);

    int real_len = strlen(formatted_name);
    while (real_len > 0 && formatted_name[real_len - 1] == ' ')
    {
      real_len--;
    }

    command_list[i] = (char *)malloc(real_len + 1);
    if (command_list[i] == 0)
    {
      for (int j = 0; j < i; j++)
        free(command_list[j]);
      free(command_list);
      close(fd);
      return 0;
    }

    memmove(command_list[i], formatted_name, real_len);
    command_list[i][real_len] = '\0';
    i++;
  }
  close(fd);

  command_list[i] = (char *)malloc(3);
  if (command_list[i] == 0)
  {
    for (int j = 0; j < i; j++)
      free(command_list[j]);
    free(command_list);
    return 0;
  }
  strcpy(command_list[i], "cd");
  i++;

  *command_count = i;

  return command_list;
}

void free_command_list(char **command_list, int count)
{
  if (command_list == 0)
    return;
  for (int i = 0; i < count; i++)
  {
    free(command_list[i]);
  }
  free(command_list);
}

#define MAX_MATCHES_LEN 1024

typedef struct
{
  int match_count;
  char all_matches_str[MAX_MATCHES_LEN];
  const char *single_match;
} MatchResult;

MatchResult
find_command_matches(const char *prefix, const char **command_list, int command_count)
{
  MatchResult result;
  const char *last_match;
  int prefix_len;
  int i, j;

  result.match_count = 0;
  result.all_matches_str[0] = '\0';
  result.single_match = 0;
  last_match = 0;

  prefix_len = strlen(prefix);
  if (prefix_len == 0)
  {
    return result;
  }

  for (i = 0; i < command_count; i++)
  {
    const char *current_command = command_list[i];
    int is_match = 1;

    for (j = 0; j < prefix_len; j++)
    {
      if (prefix[j] == '\t' || prefix[j] == '\x1b')
      {
        continue;
      }
      if (current_command[j] == '\0' || prefix[j] != current_command[j])
      {
        is_match = 0;
        break;
      }
    }

    if (is_match)
    {
      result.match_count++;
      last_match = current_command;

      if (strlen(result.all_matches_str) + strlen(current_command) + 2 < MAX_MATCHES_LEN)
      {
        char *dest_end;
        const char *src;

        dest_end = result.all_matches_str + strlen(result.all_matches_str);
        src = current_command;

        while (*src)
        {
          *dest_end++ = *src++;
        }

        *dest_end++ = ' ';

        *dest_end = '\0';
      }
    }
  }

  if (result.match_count == 1)
  {
    result.single_match = last_match;
  }

  return result;
}

int getcmd(char *buf, int nbuf)
{
  if (x == 0)
  {
    printf(2, "$ ");
  }
  else
  {
    x = 0;
  }
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if (buf[0] == 0) // EOF
    return -1;
  return 0;
}

int is_last_char_tab(const char *buf)
{
  if (!buf)
  {
    return 0;
  }
  int i = 0;
  while (buf[i] != '\0')
  {
    i++;
  }
  if (i == 0)
  {
    return 0;
  }
  if (buf[i - 1] == '\t')
  {
    return 1;
  }
  return 0;
}

int test = 0;

int main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while ((fd = open("console", O_RDWR)) >= 0)
  {
    if (fd >= 3)
    {
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0)
  {

    if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
    {
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf) - 1] = 0; // chop \n
      if (chdir(buf + 3) < 0)
        printf(2, "cannot cd %s\n", buf + 3);
      continue;
    }

    if (is_last_char_tab(buf))
    {
      int command_count;
      char **commands;

      MatchResult result;

      commands = get_all_commands(&command_count);
      if (commands == 0)
      {
        printf(2, "error: failed to get command list\n");
        exit();
      }

      result = find_command_matches(buf, (const char **)commands, command_count);

      if (result.match_count == 1)
      {
        // printf(1, "%s", buf);
        printf(1, "%s", result.single_match + strlen(buf) - 1);
      }
      else if (result.match_count > 1 && strlen(buf) > 2)
      {
        if (buf[strlen(buf) - 2] == '\x1b')
        {
          int len = strlen(buf);
          buf[len - 2] = '\0';
          printf(1, "\x01\n%s\n$ \x01%s", result.all_matches_str, buf);
        }
      }

      free_command_list(commands, command_count);

      x = 1;
      continue;
    }

    else if (fork1() == 0)
    {
      runcmd(parsecmd(buf));
    }
    wait();
  }

  exit();
}

void panic(char *s)
{
  printf(2, "%s\n", s);
  exit();
}

int fork1(void)
{
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

// PAGEBREAK!
//  Constructors

struct cmd *
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *)cmd;
}
// PAGEBREAK!
//  Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s)
  {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>')
    {
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es)
  {
    printf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&"))
  {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if (peek(ps, es, ";"))
  {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd *
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|"))
  {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>"))
  {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok)
    {
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    case '+': // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd *
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd *)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;"))
  {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd *
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type)
  {
  case EXEC:
    ecmd = (struct execcmd *)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
