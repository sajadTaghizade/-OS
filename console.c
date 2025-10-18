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
  uint r2;
  uint w2;
} input;

int tab_flag = 0;
int tab_flag2 = 0;
int tab_flag3 = 0;
int number_of_tab = 0;

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


void consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while ((c = getc()) >= 0)
  {

    if(tab_flag3 == 1) {
      int temp = input.cursor;
      input.cursor = input.e;
      move_cursor(input.e - temp);
      if(number_of_tab >= 2) {
        backspace();
      }
      backspace();
      tab_flag3 = 0;
    }

    tab_flag2 = 0;
    tab_flag = 0;
    input.r2 = 0;
    input.w2 = 0;
    switch (c)
    {
    case C('P'): // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;

    case C('U'): // Kill line.
      number_of_tab = 0;
      if (end_point != -1)
      {
        deselect(); // remove highlight from selected text MH
        break;
      }

      remove_line();
      break;
    case C('H'):
    case '\x7f': // Backspace

      number_of_tab = 0;

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

      number_of_tab = 0;

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

      number_of_tab = 0;

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

      number_of_tab = 0;

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

      number_of_tab = 0;

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

    default:
    {
      if (c != 0 && (input.e - input.w) < INPUT_BUF)
      {
        if (c == '\t')
        {
          tab_flag = 1;
          tab_flag2 = 1;
          tab_flag3 = 1;
          number_of_tab++;

          int temp = input.cursor;
          input.cursor = input.e;
          move_cursor(input.e - temp);

          if(number_of_tab >= 2) {
            input.buf[input.e++ % INPUT_BUF] = '\x1b';
          }
          input.buf[input.e++ % INPUT_BUF] = c;
          input.w2 = input.e;
          input.r2 = input.r;

          wakeup(&input.r);

        }
        else if (c != '\n')
        { // Handle normal characters
          if (end_point != -1)
          {
            delete_selected_text();
          }
          write_character(c);
        }
      }
      break;
    }
    }
  }
  release(&cons.lock);

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
    while (input.r == input.w && tab_flag == 0)
    {
      if (myproc()->killed)
      {
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      
      sleep(&input.r, &cons.lock);
    }
    if(tab_flag) {
      c = input.buf[(input.r2)% INPUT_BUF];
      if(input.r2 == input.w2) {
        tab_flag = 0;
        break;
      }
      input.r2++;

      *dst++ = c;
      --n;
      if(c == '\t') {
        input.buf[(input.r2 - 1) % INPUT_BUF] = '\0';
        break;
      }

    } else {
    
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
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int stdout = 0;

int consolewrite(struct inode *ip, char *buf, int n)
{
  if(tab_flag2 == 1 && tab_flag == 0) {
    return 1;
  } 
  else if(tab_flag == 1 && tab_flag2 == 1) {
    int i;
    iunlock(ip);
    acquire(&cons.lock);
    for (i = 0; i < n; i++) {
      if(buf[i] == 9 || buf[i] == 0) {
        continue;
      }
      if(buf[i] == '\x01') {
        stdout = 1 - stdout;
        if(stdout == 0) {
          input.r++;
          input.w++;
        }
        continue;
      }
      if(stdout == 1) {
        consputc(buf[i]  & 0xff);
      }
      else {
        write_character(buf[i] & 0xff);
      }
    }
    release(&cons.lock);
    ilock(ip);
    return n;
  }
  else {

    int i;
    iunlock(ip);
    acquire(&cons.lock);
    for (i = 0; i < n; i++) {
      consputc(buf[i] & 0xff);  
    }
    release(&cons.lock);
    ilock(ip);
    return n;
  }

}

void consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  // autocomplete_init();

  ioapicenable(IRQ_KBD, 0);
}