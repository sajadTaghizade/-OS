// Shell.

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h" // برای struct dirent و DIRSIZ
void *memset(void *, int, uint);
char *strcpy(char *, const char *);
int strncmp(const char *, const char *, uint);
char *strcat(char *, const char *);
char *strncpy(char *, const char *, int);// ----------- بخش اضافه شده برای تکمیل خودکار -----------
#define MAX_COMMANDS 100
#define MAX_MATCHES 32
#define MAX_CMD_LEN 128 // حداکثر طول دستور

// لیست پویا دستورات
char command_list[MAX_COMMANDS][DIRSIZ];
int num_commands = 0;

// لیست نتایج جستجو
char matches[MAX_MATCHES][DIRSIZ];
int match_count = 0;

// متغیرهای وضعیت برای تشخیص فشار دوم Tab
char last_prefix[MAX_CMD_LEN];
int last_cursor = -1;

// تابع برای خواندن تمام فایل‌های موجود در دایرکتوری ریشه
void get_all_commands()
{
  int fd;
  struct dirent de;

  if ((fd = open("/", O_RDONLY)) < 0)
  {
    printf(2, "sh: cannot open root directory\n");
    return;
  }

  num_commands = 0;
  while (read(fd, &de, sizeof(de)) == sizeof(de))
  {
    if (de.inum == 0)
      continue;
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;

    if (num_commands < MAX_COMMANDS)
    {
      strcpy(command_list[num_commands], de.name);
      num_commands++;
    }
  }
  close(fd);
}

// تابع برای پیدا کردن دستورات منطبق با پیشوند
void find_matches(char *prefix)
{
  int prefix_len = strlen(prefix);
  match_count = 0;

  for (int i = 0; i < num_commands; i++)
  {
    if (strncmp(prefix, command_list[i], prefix_len) == 0)
    {
      if (match_count < MAX_MATCHES)
      {
        strcpy(matches[match_count], command_list[i]);
        match_count++;
      }
    }
  }
}

// تابع برای پیدا کردن طولانی‌ترین پیشوند مشترک
int find_longest_common_prefix()
{
  if (match_count <= 0)
    return 0;

  int lcp_len = strlen(matches[0]);
  for (int i = 1; i < match_count; i++)
  {
    int j = 0;
    while (j < lcp_len && j < strlen(matches[i]) && matches[0][j] == matches[i][j])
    {
      j++;
    }
    lcp_len = j;
  }
  return lcp_len;
}
// ----------- پایان بخش اضافه شده -----------


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

int
getcmd(char *buf, int nbuf)
{
  printf(2, "$ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}
// تابع getcmd قدیمی حذف شد. منطق آن به main منتقل شده است.

// ----------- تابع main جدید با قابلیت تکمیل خودکار -----------
// ----------- تابع main کامل با مدیریت Tab و Enter -----------
int
main(void)
{
  static char buf[MAX_CMD_LEN];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Main loop to get and process commands
  while(getcmd(buf, sizeof(buf)) >= 0){
    int len = strlen(buf);

    // Check if getcmd returned because of a Tab press
    if(len > 0 && buf[len-1] == '\t'){
      // --- This is an autocomplete request ---
      buf[len-1] = '\0'; // Remove the tab character from the buffer

      // Check if this is a second consecutive tab press for the same prefix
      if (strcmp(buf, last_prefix) == 0 && (len - 1) == last_cursor && match_count > 1) {
          // It's the second press: show all options
          printf(1, "\n");
          for (int j = 0; j < match_count; j++) {
              printf(1, "%s  ", matches[j]);
          }
          printf(1, "\n$ %s", buf); // Reprint the current prompt and buffer
      } else {
          // It's the first press: find matches
          get_all_commands();
          find_matches(buf);

          if(match_count == 1){
            // If there's a single match, complete it and add a space
            strcpy(buf, matches[0]);
            strcat(buf, " ");
            printf(1, "\r$ %s", buf); // Redraw the entire line
          } else if (match_count > 1) {
            // If there are multiple matches, complete the longest common prefix
            int lcp_len = find_longest_common_prefix();
            if (lcp_len > strlen(buf)) {
                strncpy(buf, matches[0], lcp_len);
                buf[lcp_len] = '\0';
                printf(1, "\r$ %s", buf); // Redraw the line with the common prefix
            }
          } else {
            // If no matches, just redraw the current line (optional, provides feedback)
            printf(1, "\r$ %s", buf);
          }
      }
      
      // Save the current state for the next potential tab press
      strcpy(last_prefix, buf);
      last_cursor = strlen(buf);

      // Go back to the start of the loop to wait for more input, don't execute
      continue;
    }

    // --- This is a normal command (terminated by Enter) ---
    // The newline is usually part of the buffer from gets(), so we chop it.
    if(len > 0 && buf[len-1] == '\n'){
        buf[len-1] = 0;
    }

    // Handle the 'cd' command, which must run in the parent process (the shell)
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      if(chdir(buf+3) < 0)
        printf(2, "cannot cd %s\n", buf+3);
      continue; // Don't fork/exec for cd
    }
    
    // For all other commands, fork a child process to execute it
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait();
  }
  exit();
}
// ----------- پایان تابع main جدید -----------


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

// بقیه کد که مربوط به parse کردن است، بدون تغییر باقی می‌ماند
// ... (کدهای parsecmd, parseline, parsepipe و ...) ...

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
