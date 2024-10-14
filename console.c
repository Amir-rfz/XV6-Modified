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

int num_of_backs = 0;
int num_of_backs_saved = 0;
int match_history = 0;
int is_copy = 0;

static void consputc(int);

static int panicked = 0;

static struct {
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

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
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

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
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
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4

static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    for (int i = pos - 1 ; i < pos + num_of_backs ; i++)
      crt[i] = crt[i + 1];

    if(pos > 0) --pos;
  } 
  else{
    for (int i = pos + num_of_backs; i > pos ; i--)
      crt[i] = crt[i - 1];
    crt[pos++] = (c&0xff) | 0x0700;  // black on white
  }

  


  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos + num_of_backs] = ' ' | 0x0700;
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define HISTORY_SIZE 11
#define INPUT_BUF 128

struct Input{
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input,saved_input, copy_input;

struct {
  struct Input history[HISTORY_SIZE];
  int curent;
  int end;
  int size;
  int match;
} inputs;

#define C(x)  ((x)-'@')  // Control-x

//xyz
static void backwardCursor(){
  int pos;

  // get cursor position
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  // move back
  if(crt[pos - 2] != ('$' | 0x0700))
    pos--;

  // reset cursor
  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);

}

//xyz
static void forwardCursor(){
  int pos;

  // get cursor position
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  // move forward
  pos++;

  // reset cursor
  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);

}


static void shiftright(char *buf)
{
  for (int i = input.e; i > input.e - num_of_backs; i--)
  {
    buf[(i) % INPUT_BUF] = buf[(i - 1) % INPUT_BUF]; // Shift elements to the right
  }
}

static void shiftleft(char *buf)
{
  for (int i = input.e - num_of_backs - 1; i < input.e; i++)
  {
    buf[(i) % INPUT_BUF] = buf[(i + 1) % INPUT_BUF]; // Shift elements to the right
  }
  input.buf[input.e] = ' ';
}

static void shiftright_saved(char *buf)
{
  for (int i = saved_input.e; i > saved_input.e - num_of_backs_saved; i--)
  {
    buf[(i) % INPUT_BUF] = buf[(i - 1) % INPUT_BUF]; // Shift elements to the right
  }
}

static void shiftleft_saved(char *buf)
{
  for (int i = saved_input.e - num_of_backs_saved - 1; i < saved_input.e; i++)
  {
    buf[(i) % INPUT_BUF] = buf[(i + 1) % INPUT_BUF]; // Shift elements to the right
  }
  saved_input.buf[saved_input.e] = ' ';
}


//xyz
void clear_saved_input()
{
  int end = saved_input.e;
  while (end != saved_input.w && saved_input.buf[(end - 1) % INPUT_BUF] != '\n')
  {
    saved_input.e--;
  }
}


void display_clear()
{
  for (int i = 0; i < num_of_backs; i++)
    forwardCursor();
  num_of_backs = 0;
  int end = input.e;
  while (end != input.w && input.buf[(end - 1) % INPUT_BUF] != '\n')
  {
    end--;
    consputc(BACKSPACE);
  }
}

void display_saved_command(){
  for(int i = (saved_input.w); i < saved_input.e; i++){
    consputc(saved_input.buf[i]);
  }
}

//xyz
void display_command(){
  for(int i = (input.w); i < input.e; i++){
    consputc(input.buf[i]);
  }
}

//xyz
static void arrowup()
{
  if (inputs.curent == inputs.end)
  {
    inputs.history[inputs.end % HISTORY_SIZE] = input;
  }
  display_clear();
  input = inputs.history[--inputs.curent % HISTORY_SIZE];
  input.buf[--input.e] = '\0';
  display_command();
}

//xyz
static void arrowdown()
{
  if (inputs.curent < inputs.end)
  {
    display_clear();
    input = inputs.history[++inputs.curent % HISTORY_SIZE];
    if (input.e != input.w && inputs.curent != inputs.end)
      input.buf[--input.e] = '\0';
    display_command();
  }
}

int is_digit_number(char c) {
  if (c >= '0' && c <= '9' )
    return 1;
  return 0;
}

// static void copy_input()

#define KEY_UP          0xE2
#define KEY_DN          0xE3
#define KEY_LF          0xE4
#define KEY_RT          0xE5

static void  print_answer(char inp_ans) {
  shiftright(input.buf);
  shiftright_saved(saved_input.buf);
  input.buf[(input.e++ - num_of_backs) % INPUT_BUF] = inp_ans;
  if(is_copy == 1)
    saved_input.buf[(saved_input.e++ - num_of_backs_saved) % INPUT_BUF] = inp_ans;
  consputc(inp_ans);
}

//xyz
void consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case KEY_UP: 
        if (inputs.size && inputs.end - inputs.curent < inputs.size && 1)
          arrowup();
          // moveCursorUp();
      break;
    case KEY_DN:
        if (1)  
          arrowdown();
        // moveCursorDown();
      break;
    case KEY_LF:  // Cursor Backward
        if((input.e - num_of_backs) > input.w){
          backwardCursor();
          num_of_backs++;
        }
        if((saved_input.e - num_of_backs_saved) > saved_input.w && is_copy == 1){
          num_of_backs_saved++;
        }
      break;
      case KEY_RT:  // Cursor Backward
        if(num_of_backs > 0){
          forwardCursor();
          num_of_backs--;
        }
        if(num_of_backs_saved > 0 && is_copy == 1){
          num_of_backs_saved--;
        }
        break;
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      num_of_backs = 0;
      num_of_backs_saved = 0;
      break;
    case C('S'):
      is_copy = 1;
      break;
    case C('F'):
      is_copy = 0;
      display_saved_command();
      for(int i = (saved_input.w); i < saved_input.e; i++){
        input.buf[(input.e++ - num_of_backs) % INPUT_BUF] = saved_input.buf[i];
      }
      saved_input.e = saved_input.w;
      num_of_backs_saved = 0;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): 
    case '\x7f':  // Backspace
      if(input.e != input.w && input.e - input.w > num_of_backs){
        if (num_of_backs > 0)
          shiftleft(input.buf);
        input.e--;
        consputc(BACKSPACE);
      }
      if(saved_input.e != saved_input.w && saved_input.e - saved_input.w > num_of_backs_saved && is_copy == 1){
        if (num_of_backs_saved > 0)
          shiftleft_saved(saved_input.buf);
        saved_input.e--;
      }
      break;
    default:

      if(input.e >= 5) {
        int match_equation = 1;
        int ans = 0;
        int num1 = 0;
        int num2 = 0;
        int edit_place = input.e - num_of_backs;
        if (is_digit_number(input.buf[edit_place-5]) && is_digit_number(input.buf[edit_place-3]) && input.buf[edit_place-2] == '=' && input.buf[edit_place-1] == '?') {
          num1 = input.buf[edit_place-5] - '0';
          num2 = input.buf[edit_place-3] - '0';
          if (input.buf[edit_place-4] == '+')
            ans = num1 + num2;
          else if (input.buf[edit_place-4] == '-')
            ans = num1 - num2;
          else if (input.buf[edit_place-4] == '*')
            ans = num1 * num2;
          else if (input.buf[edit_place-4] == '/')
            ans = num1 / num2;
          else
            match_equation = 0;         
        }
        else {
          match_equation = 0;
        }
        if(match_equation == 1) {
          for(int i=0;i<5;i++){
            if(input.e != input.w && input.e - input.w > num_of_backs){
              if (num_of_backs > 0)
                shiftleft(input.buf);
              input.e--;
              consputc(BACKSPACE);
              }
            if(saved_input.e != saved_input.w && saved_input.e - saved_input.w > num_of_backs_saved && is_copy == 1) {
              if (num_of_backs_saved > 0)
                shiftleft_saved(saved_input.buf);
              saved_input.e--;
            }
          }
          char inp_ans = '0';
          if (ans < 0) {
            print_answer('-');
            ans = 0-ans;
          }
          if (ans > 9) {
            inp_ans = '0' + (ans / 10);
            print_answer(inp_ans);
          }
          inp_ans = '0' + (ans % 10);
          print_answer(inp_ans);
          }
        }
      

      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;

        if(c == '\n') {
          num_of_backs = 0;
          num_of_backs_saved = 0;
          match_history = 0;
          if((input.e - input.w) == 7){
            if (match_history == 0) {
              match_history = 1;
              char *history_cmd = "history";
              for(int i=input.w, j=0; i< input.e; i++, j++) {
                if(input.buf[i] != history_cmd[j])
                  match_history = 0;
              }
              copy_input = input;
              if(match_history == 1){
                consputc('\n');
                for(int i=0; i<inputs.size; i++){
                  input = inputs.history[i];
                  input.buf[--input.e] = '\0';
                  display_command();
                  if (i != inputs.size - 1)
                    consputc('\n');
                }
                input = copy_input;
              }
            }
          }
        }


        shiftright(input.buf);
        shiftright_saved(saved_input.buf);
        input.buf[(input.e++ - num_of_backs) % INPUT_BUF] = c;
        if(is_copy == 1)
          saved_input.buf[(saved_input.e++ - num_of_backs_saved) % INPUT_BUF] = c;
        consputc(c);
        if((c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF) && 1){
          if (inputs.size <= 10)
            inputs.history[inputs.end++ % HISTORY_SIZE] = input;
          else {
            for (int i = 0; i < inputs.size; i++)
            {
              inputs.history[i] = inputs.history[i+1];
            }
            inputs.history[inputs.size - 1] = input; 
          }
          inputs.curent = inputs.end;
          if (inputs.size < 10)
            inputs.size++;
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

//xyz
int consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

//xyz
int consolewrite(struct inode *ip, char *buf, int n)
{
  int i;
  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);
  return n;
}

//xyz
void consoleinit(void)
{
  initlock(&cons.lock, "console");
  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;
  ioapicenable(IRQ_KBD, 0);
}