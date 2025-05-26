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
int copy_is_end = 0;

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void printint(int xx, int base, int sign)
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
void cprintf(char *fmt, ...)
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

static void cgaputc(int c)
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
    crt[pos++] = (c&0xff) | 0x0700; // black on white
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

static void backward_cursor(){
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

static void forward_cursor(){
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


static void shift_right(char *buf)
{
  for (int i = input.e; i > input.e - num_of_backs; i--)
  {
    buf[(i) % INPUT_BUF] = buf[(i - 1) % INPUT_BUF];
  }
}

static void shift_left(char *buf)
{
  for (int i = input.e - num_of_backs - 1; i < input.e; i++)
  {
    buf[(i) % INPUT_BUF] = buf[(i + 1) % INPUT_BUF];
  }
  input.buf[input.e] = ' ';
}

static void shift_right_saved(char *buf)
{
  for (int i = saved_input.e; i > saved_input.e - num_of_backs_saved; i--)
  {
    buf[(i) % INPUT_BUF] = buf[(i - 1) % INPUT_BUF];
  }
}

static void shift_left_saved(char *buf)
{
  for (int i = saved_input.e - num_of_backs_saved - 1; i < saved_input.e; i++)
  {
    buf[(i) % INPUT_BUF] = buf[(i + 1) % INPUT_BUF];
  }
  saved_input.buf[saved_input.e] = ' ';
}

void clear_line()
{
  for (int i = 0; i < num_of_backs; i++)
    forward_cursor();
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

void display_command(){
  for(int i = (input.w); i < input.e; i++){
    consputc(input.buf[i]);
  }
}

static void arrow_up()
{
  if (inputs.curent == inputs.end)
  {
    inputs.history[inputs.end % HISTORY_SIZE] = input;
  }
  clear_line();
  input = inputs.history[--inputs.curent % HISTORY_SIZE];
  input.buf[--input.e] = '\0';
  display_command();
}

static void arrow_down()
{
  if (inputs.curent < inputs.end)
  {
    clear_line();
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

int is_operator(char c) {
  if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%')
    return 1;
  return 0;
}

int get_num(int index_num, int index_end ) {
  int num = 0;
  for (int i = index_num ; i <= index_end ; i++) {
    num *= 10;
    num += (input.buf[i] - '0');
  }
  return num;
}

float get_answer(int index_num1, int index_num2, int edit_place, int is_num1_neg) {
    int num1 = get_num(index_num1, index_num2-2);
    int num2 = get_num(index_num2, edit_place-3);
    float float_ans = 0;
    if (is_num1_neg == 1)
      num1 = 0-num1;
    if (input.buf[index_num2 - 1] == '+')
      float_ans = num1 + num2;
    else if (input.buf[index_num2 - 1] == '-')
      float_ans = num1 - num2;
    else if (input.buf[index_num2 - 1] == '*')
      float_ans = num1 * num2;
    else if (input.buf[index_num2 - 1] == '%')
      float_ans = num1 % num2;
    else if (input.buf[index_num2 - 1] == '/') 
      float_ans = (float)num1 / num2; 
    return float_ans;
}

void remove_equation(int remove_num) {
  for(int i=0;i<remove_num;i++){
    if(input.e != input.w && input.e - input.w > num_of_backs){
      if (num_of_backs > 0)
        shift_left(input.buf);
      input.e--;
      consputc(BACKSPACE);
      }
    if(saved_input.e != saved_input.w && saved_input.e - saved_input.w > num_of_backs_saved && is_copy == 1){
      if (num_of_backs_saved > 0)
        shift_left_saved(saved_input.buf);
      saved_input.e--;
    }
  }
}

static void  print_char(char inp_ans) {
  shift_right(input.buf);
  shift_right_saved(saved_input.buf);
  input.buf[(input.e++ - num_of_backs) % INPUT_BUF] = inp_ans;
  if(is_copy == 1)
    saved_input.buf[(saved_input.e++ - num_of_backs_saved) % INPUT_BUF] = inp_ans;
  consputc(inp_ans);
}


void print_number(int ans) {
  if (ans <= 0)
    return;
  print_number(ans / 10);
  print_char('0' + (ans % 10));
}

void print_answer(float float_ans, int index_num2) {
  char float_part = '0';
  if (float_ans < 0) {
    print_char('-');
    float_ans = 0-float_ans;
  }
  if (input.buf[index_num2 - 1] == '/') {
    int temp = float_ans * 10;
    float_part = '0' + (temp % 10);
  }
  if (float_ans < 1)
    print_char('0');
  else
    print_number((int)float_ans);
  if (input.buf[index_num2 - 1] == '/') {
    print_char('.');
    print_char(float_part);
  } 
}

#define KEY_UP          0xE2
#define KEY_DN          0xE3
#define KEY_LF          0xE4
#define KEY_RT          0xE5

void consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case KEY_UP:
        if (inputs.size && inputs.end - inputs.curent < inputs.size && 1)
          arrow_up();
      break;
    case KEY_DN:
        arrow_down();
      break;
    case KEY_LF:
        if((input.e - num_of_backs) > input.w){
          backward_cursor();
          num_of_backs++;
        }
        if((saved_input.e - num_of_backs_saved) > saved_input.w && is_copy == 1){
          num_of_backs_saved++;
        }
      break;
      case KEY_RT:
        if(num_of_backs > 0){
          forward_cursor();
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
      saved_input.e = saved_input.w;
      num_of_backs_saved = 0;
      is_copy = 1;
      copy_is_end = 0;
      break;
    case C('F'):
      if (copy_is_end == 0) {
        copy_is_end = 1;
        is_copy = 0;
        break;
      }
      is_copy = 0;
      display_saved_command();
      for(int i = (saved_input.w); i < saved_input.e; i++){
        input.buf[(input.e++ - num_of_backs) % INPUT_BUF] = saved_input.buf[i];
      }
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
          shift_left(input.buf);
        input.e--;
        consputc(BACKSPACE);
      }
      if(saved_input.e != saved_input.w && saved_input.e - saved_input.w > num_of_backs_saved && is_copy == 1){
        if (num_of_backs_saved > 0)
          shift_left_saved(saved_input.buf);
        saved_input.e--;
      }
      break;
    default:

      if(input.e >= 5) {
        int edit_place = input.e - num_of_backs;
        int match_equation = 0;
        int state_machine = 0;
        int is_num1_neg = 0;
        int index_num1 = 0;
        int index_num2 = 0;

        if (is_digit_number(input.buf[edit_place-3]) && input.buf[edit_place-2] == '=' && input.buf[edit_place-1] == '?') {
          for (int i = edit_place -3 ; i >= input.w ; i--){
            if (state_machine == 0){
              if (is_digit_number(input.buf[i]))
                continue;
              else if(is_operator(input.buf[i])){
                state_machine = 1;
                index_num2 = i+1;
              }
              else
                break;
            }
            else if (state_machine == 1 && is_digit_number(input.buf[index_num2-2])){ 
              match_equation = 1;
              index_num1 = i;
              if (is_digit_number(input.buf[i]))
                continue;
              else if (input.buf[i] == '-') {
                index_num1 = i+1;
                is_num1_neg = 1;
                break;
              }
              else {
                index_num1 = i+1;
                break;
              }
            }
          }
        }

        if (match_equation == 1) {
          float float_ans = get_answer(index_num1, index_num2, edit_place, is_num1_neg);
          int num_remove = edit_place - index_num1 + is_num1_neg;
          remove_equation(num_remove);  
          print_answer(float_ans, index_num2);     
        }        
      }


      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        if(c == '\n') {
          num_of_backs = 0;
          num_of_backs_saved = 0;
          match_history = 0;
          if((input.e - input.w) >= 7){
            if (match_history == 0) {
              match_history = 1;
              char *history_cmd = "history";
              for(int i=input.w, j=0; i< input.e; i++, j++) {
                if(input.buf[i] != history_cmd[j] && j <= 6)
                  match_history = 0;
                else if (j >= 7 && input.buf[i] != ' ')
                  match_history = 0;
              }
              copy_input = input;
              if(match_history == 1){
                if (inputs.size != 0)
                  consputc('\n');
                for(int i=0; i<inputs.size; i++){
                  input = inputs.history[(inputs.end - inputs.size + i) % HISTORY_SIZE];
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


        shift_right(input.buf);
        shift_right_saved(saved_input.buf);
        input.buf[(input.e++ - num_of_backs) % INPUT_BUF] = c;
        if(is_copy == 1)
          saved_input.buf[(saved_input.e++ - num_of_backs_saved) % INPUT_BUF] = c;
        consputc(c);
        if((c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF) && 1){
          inputs.history[inputs.end++ % HISTORY_SIZE] = input;
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

void consoleinit(void)
{
  initlock(&cons.lock, "console");
  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;
  ioapicenable(IRQ_KBD, 0);
}

void print_blank(int num_of_space)
{
  for(int i = 0; i < num_of_space; i++){
    cprintf(" ");
  }
}

int find_length(int number) {
  int count = 0;
  if (number == 0) {
    return 1;
  }
  while(number != 0) {
    number /= 10;
    count++;
  }
  return count;
}