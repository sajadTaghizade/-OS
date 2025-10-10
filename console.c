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

static void consputc(int);

static int panicked = 0;

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

#define INPUT_BUF 128

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
  if (end_point != -1)
  {
    ushort attr = 0x0700;

    for (int i = start_point; i <= end_point; i++)
    {
      crt[i] = (crt[i] & 0x00FF) | attr;
    }
    start_point = -1;
    end_point = -1;
  }
}

static int is_space(char c){
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static void
backspace()
{
  if (input.cursor == input.w)
    return;

  uint original_pos = read_cursor_pos() - 1;

  input.cursor--;

  memmove(&input.buf[input.cursor % INPUT_BUF],
          &input.buf[(input.cursor + 1) % INPUT_BUF],
          input.e - (input.cursor + 1));

  input.e--;

  write_cursor_pos(original_pos);

  for (int i = input.cursor; i < input.e; i++)
  {
    consputc(input.buf[i % INPUT_BUF]);
  }

  consputc(' ');

  write_cursor_pos(original_pos);
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

// end of extra functions MH

void consoleintr(int (*getc)(void))
{
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
      while (input.e != input.w &&
             input.buf[(input.e - 1) % INPUT_BUF] != '\n')
      {
        input.e--;
        consputc(BACKSPACE);
      }
      start_point = -1;
      end_point = -1;
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
      if (input.cursor > input.w)
      {
        input.cursor--;
        uint pos = read_cursor_pos();
        pos--;
        write_cursor_pos(pos);
      }
      deselect(); // remove highlight from selected text MH
      break;

    case KEY_RT:
      if (input.cursor < input.e)
      {
        input.cursor++;
        uint pos = read_cursor_pos();
        pos++;
        write_cursor_pos(pos);
      }
      deselect(); // remove highlight from selected text MH
      break;

    case C('D'):
    { 

      if(input.e == input.w){
        input.w == input.e;
        wakeup(&input.r);
        deselect();
        break;
      }

      int i = input.cursor ;

      while( i < input.e && !is_space(input.buf[i % INPUT_BUF])) i++ ;

      while( i < input.e && is_space(input.buf[i % INPUT_BUF])) i++ ;

      if (i > input.cursor){
        for( int k = input.cursor){
          consputc(input.buf[ k % INPUT_BUF]);
        }
        input.cursor = i ; 

      }

      deselect(); 
      break;

    }


    case C('A'):
    {
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
      deselect(); // remove highlight from selected text MH
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
        delete_selected_text();
        int i = 0;
        while (copy_buffer[i] != '\0' && input.e < input.w + INPUT_BUF)
        {
          char char_to_paste = copy_buffer[i];

          // 1. Add the character to the input data buffer
          input.buf[input.e % INPUT_BUF] = char_to_paste;
          input.e++;

          // 2. Update the internal cursor position
          input.cursor++;

          // 3. Display the character on the screen
          consputc(char_to_paste);

          i++;
        }
      }
      break;

      // end of new cases MH

    default:
      if (c != 0 && input.e - input.r < INPUT_BUF)
      {
        c = (c == '\r') ? '\n' : c;

        if (end_point != -1)
        {
          delete_selected_text(); // delete selected text if any MH
        }

        input.buf[input.e++ % INPUT_BUF] = c;
        input.cursor++;
        consputc(c);
        if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF)
        {
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if (doprocdump)
  {
    procdump(); // now call procdump() wo. cons.lock held
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

  ioapicenable(IRQ_KBD, 0);
}
