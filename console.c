// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
// #include "string.h"
#include "kbd.h"

#define INPUT_BUF 128

static void consputc(int);
static int stamp[INPUT_BUF];
static int panicked = 0;
static int ins_tick = 0;
static struct
{
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if (sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do
  {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}
// PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if (locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint *)(void *)(&fmt + 1);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
  {
    if (c != '%')
    {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c)
    {
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if ((s = (char *)*argp++) == 0)
        s = "(null)";
      for (; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if (locking)
    release(&cons.lock);
}

void panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for (i = 0; i < 10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for (;;)
    ;
}

// PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort *)P2V(0xb8000); // CGA memory

static void
gaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);

  if (c == '\n')
    pos += 80 - pos % 80;
  else if (c == BACKSPACE)
  {
    if (pos > 0)
      --pos;
  }
  else
    crt[pos++] = (c & 0xff) | 0x0700; // black on white

  if (pos < 0 || pos > 25 * 80)
    panic("pos under/overflow");

  if ((pos / 80) >= 24)
  { // Scroll up.
    memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
    pos -= 80;
    memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
  crt[pos] = ' ' | 0x0700;
}

// extra variables MH

static int start_point = -1;

static int end_point = -1;

static char copy_buffer[INPUT_BUF];

// end of extra variables MH

void consputc(int c)
{
  if (panicked)
  {
    cli();
    for (;;)
      ;
  }

  if (c == BACKSPACE)
  {
    uartputc('\b');
    uartputc(' ');
    uartputc('\b');
  }
  else
    uartputc(c);
  gaputc(c);
}

struct
{
  char buf[INPUT_BUF];
  uint r; // Read index
  uint w; // Write index
  uint e; // Edit index
  uint cursor;
} input;

// #define C(x)  ((x)-'@')  // Control-x

// extra functions$

static uint
read_cursor_pos(void)
{
  uint pos;
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);
  return pos;
}

static void
write_cursor_pos(uint pos)
{
  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
}

static void
move_cursor(int steps)
{
  if (steps == 0)
    return;
  uint pos = read_cursor_pos();
  pos += steps;
  write_cursor_pos(pos);
}
// end of extra functions$

// extra functions MH

static void
consolehighlight(int start_pos, int end_pos, int on)
{
  int max_pos = 25 * 80;
  ushort attr;

  if (start_pos < 0 || end_pos >= max_pos || start_pos > end_pos)
  {
    return;
  }

  if (on)
  {
    attr = 0x7000; // Highlight ON: Black text on light-grey background
  }
  else
  {
    attr = 0x0700; // Highlight OFF: Default light-grey text on black background
  }

  for (int i = start_pos; i <= end_pos; i++)
  {
    crt[i] = (crt[i] & 0x00FF) | attr;
  }
}

static void
deselect()
{
  ushort attr = 0x0700;

  for (int i = start_point; i <= end_point; i++)
  {
    crt[i] = (crt[i] & 0x00FF) | attr;
  }
  start_point = -1;
  end_point = -1;
}

static void
backspace()
{
  if (input.cursor == input.w)
    return;

  uint original_pos = read_cursor_pos() - 1;

  input.cursor--;
  for (unsigned j = input.cursor; j < input.e - 1; j++)
  {
    input.buf[j % INPUT_BUF] = input.buf[(j + 1) % INPUT_BUF];
  }

  for (unsigned j = input.cursor; j < input.e - 1; j++)
  {
    stamp[j % INPUT_BUF] = stamp[(j + 1) % INPUT_BUF];
  }
  input.e--;
  stamp[input.e % INPUT_BUF] = 0;

  write_cursor_pos(original_pos);

  for (int i = input.cursor; i < input.e; i++)
  {
    consputc(input.buf[i % INPUT_BUF]);
  }

  consputc(' ');

  write_cursor_pos(original_pos);
}

static void
delete_char_at(int index)
{
  int temp = input.cursor;
  input.cursor = index + 1;
  uint current_pos = read_cursor_pos();
  current_pos += input.cursor - temp;
  write_cursor_pos(current_pos);
  backspace();
}

static void
delete_selected_text()
{
  int current_pos = read_cursor_pos();
  if (current_pos == start_point)
  {
    int step = end_point - start_point + 1;
    input.cursor += step;
    current_pos += step;
    write_cursor_pos(current_pos);
  }
  for (int i = 0; i < end_point - start_point + 1; i++)
  {
    backspace();
  }
  start_point = -1;
  end_point = -1;
}

static void
write_character(char c)
{
  uint original_pos = read_cursor_pos();

  for (unsigned j = input.e; j > input.cursor; j--)
  {
    input.buf[j % INPUT_BUF] = input.buf[(j - 1) % INPUT_BUF];
    stamp[j % INPUT_BUF] = stamp[(j - 1) % INPUT_BUF];
  }

  input.buf[input.cursor % INPUT_BUF] = c;
  stamp[input.cursor % INPUT_BUF] = ++ins_tick;

  input.e++;
  input.cursor++;

  for (int i = input.cursor - 1; i < input.e; i++)
  {
    consputc(input.buf[i % INPUT_BUF]);
  }

  write_cursor_pos(original_pos + 1);
}
// end of extra functions MH

#define MAX_MATCHES 32
static char last_prefix[INPUT_BUF];
static char matches[MAX_MATCHES][DIRSIZ]; // DIRSIZ حداکثر طول نام فایل است
static int match_count = 0;
static int last_cursor = -1;
#define MAX_COMMANDS 64 // حداکثر تعداد دستورات
static char command_list[MAX_COMMANDS][DIRSIZ];
static int num_commands = 0;

// این تابع لیست دستورات را در هنگام بوت بارگذاری می‌کند
// نسخه نهایی و امن: لیست دستورات را به صورت دستی تعریف می‌کند
static void
autocomplete_init(void)
{
  num_commands = 0; // ریست کردن شمارنده

  // لیست تمام دستورات پیش‌فرض xv6
  char *cmds[] = {
      "cat", "echo", "forktest", "grep", "kill", "ln", "ls", "mkdir",
      "rm", "sh", "stressfs", "usertests", "wc", "zombie", "zobie"
      // اگر برنامه جدیدی مثل find_sum اضافه کردید، نام آن را هم اینجا اضافه کنید
      // , "find_sum"
  };

  int num_cmds_to_load = sizeof(cmds) / sizeof(cmds[0]);

  for (int i = 0; i < num_cmds_to_load && i < MAX_COMMANDS; i++)
  {
    safestrcpy(command_list[num_commands], cmds[i], DIRSIZ);
    num_commands++;
  }
}
// ...

// تابع برای جستجو در فایل سیستم
// Corrected function for searching in the file system from the kernel
// نسخه نهایی و سریع find_matches

static void
find_matches(char *prefix)
{
  int prefix_len = strlen(prefix);
  match_count = 0;

  for (int i = 0; i < num_commands; i++)
  {
    if (strncmp(prefix, command_list[i], prefix_len) == 0)
    {
      if (match_count < MAX_MATCHES)
      {
        safestrcpy(matches[match_count], command_list[i], DIRSIZ);
        match_count++;
      }
    }
  }
}

// تابع جدید برای پیدا کردن بلندترین پیشوند مشترک
static int
find_longest_common_prefix(void)
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

static void
remove_line()
{
  int current_pos = read_cursor_pos();
  current_pos += input.e - input.cursor;
  input.cursor = input.e;
  write_cursor_pos(current_pos);
  while (input.e != input.w &&
         input.buf[(input.e - 1) % INPUT_BUF] != '\n')
  {
    input.e--;
    consputc(BACKSPACE);
  }
  input.w = input.e;
  input.cursor = input.w;
  start_point = -1;
  end_point = -1;
}

static void
handle_autocomplete()
{
  // ۱. استخراج پیشوند فعلی
  // int current_pos = read_cursor_pos();
  // current_pos += input.e - input.cursor;
  // input.cursor = input.e;
  // write_cursor_pos(current_pos);
  char prefix[INPUT_BUF];
  int i = input.e;
  while (i > input.w)
  {
    i--;
  }

  int prefix_len = input.e - i;
  memmove(prefix, &input.buf[i % INPUT_BUF], prefix_len);
  prefix[prefix_len] = '\0';

  if ((strlen(prefix) == strlen(last_prefix)) && (strncmp(prefix, last_prefix, prefix_len) == 0) && (input.cursor == last_cursor))
  {
    if (match_count > 1)
    {
      // to handele  Unauthorized access to the memory BEGIN
      release(&cons.lock);

      cprintf("\n");
      for (int j = 0; j < match_count; j++)
      {
        cprintf("%s  ", matches[j]);
      }
      cprintf("\n");
      for (int k = input.w; k < input.e; k++)
        consputc(input.buf[k % INPUT_BUF]);
      // to handele  Unauthorized access to the memory END
      acquire(&cons.lock);
    }
    return;
  }

  find_matches(prefix);

  if (match_count == 1)
  {
    remove_line();
    int len = strlen(matches[0]);
    for (int j = 0; j < len; j++)
      write_character(matches[0][j]);
    write_character(' ');
  }
  else if (match_count > 1)
  {
    int lcp_len = find_longest_common_prefix();
    int remaining_len = lcp_len - prefix_len;

    if (remaining_len > 0)
    {
      for (int j = 0; j < remaining_len; j++)
        write_character(matches[0][prefix_len + j]);
    }

    safestrcpy(last_prefix, prefix, INPUT_BUF);
    last_cursor = input.cursor;
  }
}

void consoleintr(int (*getc)(void))
{
  int print_debug = 0;
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while ((c = getc()) >= 0)
  {
    switch (c)
    {
    case C('P'): // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;

    case C('U'): // Kill line.
      if (end_point != -1)
      {
        deselect(); // remove highlight from selected text MH
        break;
      }

      remove_line();
      break;
    case C('H'):
    case '\x7f': // Backspace
      if (end_point == -1)
      {
        backspace();
      }
      else
      {
        delete_selected_text();
      }
      break;

      // new cases$
    case KEY_LF:
      if (end_point != -1)
      {
        deselect(); // remove highlight from selected text MH
        break;
      }

      if (input.cursor > input.w)
      {
        input.cursor--;
        uint pos = read_cursor_pos();
        pos--;
        write_cursor_pos(pos);
      }
      break;

    case KEY_RT:
      if (end_point != -1)
      {
        deselect(); // remove highlight from selected text MH
        break;
      }

      if (input.cursor < input.e)
      {
        input.cursor++;
        uint pos = read_cursor_pos();
        pos++;
        write_cursor_pos(pos);
      }
      break;

    case C('D'):
      if (end_point != -1)
      {
        deselect(); // remove highlight from selected text MH
        break;
      }

      if (input.cursor < input.e)
      {
        int i = input.cursor;
        // go to end of current word$
        while (i < input.e && input.buf[i % INPUT_BUF] != ' ')
        {
          i++;
        }
        // go to end of space(s)$
        while (i < input.e && input.buf[i % INPUT_BUF] == ' ')
        {
          i++;
        }

        int steps = i - input.cursor;
        // move logical and physical cursor based on step(s)$
        if (steps > 0)
        {
          input.cursor = i;

          move_cursor(steps);
        }
      }
      break;

    case C('A'):
    {
      if (end_point != -1)
      {
        deselect(); // remove highlight from selected text MH
        break;
      }

      int current_pos = input.cursor;
      int beginning_of_word = current_pos;
      // go to beginning of word$
      while (beginning_of_word > input.w && input.buf[(beginning_of_word - 1) % INPUT_BUF] != ' ')
      {
        beginning_of_word--;
      }
      // condition of jumping to the beginning of word$
      if (current_pos == beginning_of_word)
      {
        while (beginning_of_word > input.w && input.buf[(beginning_of_word - 1) % INPUT_BUF] == ' ')
        {
          beginning_of_word--;
        }
        while (beginning_of_word > input.w && input.buf[(beginning_of_word - 1) % INPUT_BUF] != ' ')
        {
          beginning_of_word--;
        }
      }

      int steps = current_pos - beginning_of_word;

      if (steps > 0)
      {
        input.cursor = beginning_of_word;

        move_cursor(-steps);
      }

      break;
    }

    // end of new cases$

    // new cases MH
    case C('S'): // Ctrl+S for select
    {
      int current_pos = read_cursor_pos();

      if (start_point == -1)
      {
        // This is the FIRST press of Ctrl+S, mark the start point
        start_point = current_pos;
      }
      else
      {
        // This is the SECOND press, mark the end and highlight
        end_point = current_pos - 1;

        // Ensure start_point is always less than end_point
        if (start_point > end_point)
        {
          int temp = start_point;
          start_point = end_point;
          end_point = temp;
          start_point++;
          end_point--;
        }

        // Call the function to highlight the region
        consolehighlight(start_point, end_point, 1);
      }
      break;
    }

    case C('C'): // Copy selected text
      if (start_point != -1 && end_point != -1)
      {
        int i, j;
        // Loop from the start to the end of the selection
        for (i = start_point, j = 0; i <= end_point && j < INPUT_BUF - 1; i++, j++)
        {
          // Read the character byte (low byte) from video memory
          // and store it in our copy buffer.
          copy_buffer[j] = crt[i] & 0xFF;
        }
        // Add a null terminator to make it a valid C string.
        copy_buffer[j] = '\0';
      }
      break;

    case C('V'): // Paste text from copy_buffer
      if (copy_buffer[0] != '\0')
      { // Check if there's anything to paste
        if (end_point != -1)
        {
          delete_selected_text();
        }
        int i = 0;
        while (copy_buffer[i] != '\0' && input.e < input.w + INPUT_BUF)
        {
          char char_to_paste = copy_buffer[i];

          // 1. Add the character to the input data buffer
          write_character(char_to_paste);

          i++;
        }
      }
      break;
      // end of new cases MH


    case C('Z'):
    {
      if (end_point != -1)
      {
        deselect(); // remove highlight from selected text MH
        break;
      }

      if (input.e == input.w)
        break;

      int latest_stamp = -1;
      int index_to_remove = -1;

      for (int i = input.w; i < input.e; i++)
      {
        if (stamp[i % INPUT_BUF] > latest_stamp)
        {
          latest_stamp = stamp[i % INPUT_BUF];
          index_to_remove = i;
        }
      }

      if (index_to_remove != -1)
      {
        delete_char_at(index_to_remove);
      }
      break;
    }

    case '\t': 
      handle_autocomplete();
      break;


    default:
    {
      if (c != 0 && (input.e - input.w) < INPUT_BUF)
      {
        if (c != '\n')
        {
          if (end_point != -1)
          {
            delete_selected_text();
          }
          write_character(c);
        }
        else
        {
          input.buf[input.e++] = c;
          consputc(c);
          if (c == '\n')
          {
            input.w = input.e;
            input.cursor = input.e;
            wakeup(&input.r);
            ins_tick = 0;
          }
        }
      }
      break;
    }
    }
  }
  release(&cons.lock);

  //  debug
  if (print_debug)
  {
    cprintf("\nDEBUG\n");
    cprintf("Buffer: [");
    for (int i = input.w; i < input.e; i++)
    {
      cprintf("%c", input.buf[i % INPUT_BUF]);
    }
    cprintf("]\nStamps: [");
    for (int i = input.w; i < input.e; i++)
    {
      cprintf("%d ", stamp[i % INPUT_BUF]);
    }
    cprintf("]\n");
    cprintf("------------------------\n");
  }

  if (doprocdump)
  {
    procdump();
  }
}

int consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while (n > 0)
  {
    while (input.r == input.w)
    {
      if (myproc()->killed)
      {
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if (c == C('D'))
    { // EOF
      if (n < target)
      {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if (c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for (i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  autocomplete_init();

  ioapicenable(IRQ_KBD, 0);
}