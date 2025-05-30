#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"
#include "spinlock.h"


struct {
  struct spinlock lock;
  int count;
} total_syscallcount;

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
  struct proc *curproc = myproc();

  if(addr >= curproc->sz || addr+4 > curproc->sz)
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

void
init_total_syscall_count(void)
{
  initlock(&total_syscallcount.lock, "total_syscallcount");
  total_syscallcount.count = 0;
}

int 
get_total_syscallcount(void)
{
  return total_syscallcount.count;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
  char *s, *ep;
  struct proc *curproc = myproc();

  if(addr >= curproc->sz)
    return -1;
  *pp = (char*)addr;
  ep = (char*)curproc->sz;
  for(s = *pp; s < ep; s++){
    if(*s == 0)
      return s - *pp;
  }
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes.  Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, int size)
{
  int i;
  struct proc *curproc = myproc();
 
  if(argint(n, &i) < 0)
    return -1;
  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)
    return -1;
  *pp = (char*)i;
  return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
  int addr;
  if(argint(n, &addr) < 0)
    return -1;
  return fetchstr(addr, pp);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_create_palindrome(void);
extern int sys_move_file(void);
extern int sys_sort_syscalls(void);
extern int sys_get_most_invoked_syscall(void);
extern int sys_list_all_processes(void);
extern int sys_change_scheduling_queue(void);
extern int sys_print_processes_info(void);
extern int sys_set_sjf_params(void);
extern int sys_getsyscallcount(void);
extern int sys_testreentrantlock(void);
extern int sys_open_shared_memory(void);
extern int sys_close_shared_memory(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_create_palindrome] sys_create_palindrome,
[SYS_move_file] sys_move_file,
[SYS_sort_syscalls] sys_sort_syscalls,
[SYS_get_most_invoked_syscall] sys_get_most_invoked_syscall,
[SYS_list_all_processes] sys_list_all_processes,
[SYS_change_scheduling_queue] sys_change_scheduling_queue,
[SYS_print_processes_info] sys_print_processes_info,
[SYS_set_sjf_params] sys_set_sjf_params,
[SYS_getsyscallcount] sys_getsyscallcount,
[SYS_testreentrantlock] sys_testreentrantlock,
[SYS_open_shared_memory]  sys_open_shared_memory,
[SYS_close_shared_memory]  sys_close_shared_memory
};

const char *syscall_names[] = {"fork", "exit", "wait", "pipe", "read", "kill", "exec", "fstat", "chdir", "dup", 
                              "getpid", "sbrk", "sleep", "uptime", "open", "write", "mknod", "unlink", "link", 
                              "mkdir", "close", "create_palindrome", "move_file", "sort_syscalls", 
                              "get_most_invoked_syscall", "list_all_processes"};

int record_syscall(struct proc *p, int num) {
  for (int i = 0; i < MAX_SYSCALLS; i++) {
    if (p->syscall_data[i].number == num) {
      p->syscall_data[i].count++;
      p->syscall_counts++;
      return 1;
    }
    if (p->syscall_data[i].count == 0) {
      break;
    }
  }

  for (int i = 0; i < MAX_SYSCALLS; i++) {
    if (p->syscall_data[i].count == 0) {
      p->syscall_data[i].number = num;
      p->syscall_data[i].count = 1;
      p->syscall_data[i].name = syscall_names[num-1];
      p->syscall_counts++;
      return 0;
    }
  }

  cprintf("Error: syscall_data array full for PID %d\n", p->pid);
  return -1;
}

int get_coefficient(int pid) {
  if (pid == 15) {
    return 3;
  }
  else if (pid == 16) {
    return 2;
  }
  else {
    return 1;
  }
}

void syscall(void) {
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  int coeff = get_coefficient(num);

  pushcli();
  mycpu()->syscall_count += coeff;
  popcli();

  acquire(&total_syscallcount.lock);
  total_syscallcount.count += coeff;
  release(&total_syscallcount.lock);


  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    if (num < MAX_SYSCALLS) {
        record_syscall(curproc, num);
    }

    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n", curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}