#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"


int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_create_palindrome(void)
{
  int number = myproc()->tf->ebx;
  cprintf("Kernel: Executing palindrome generation system call with input: %d\n", number);
  create_palindrome(number);

  return 0;
}

// struct proc*
// find_proc(int pid)
// {
//   struct proc *p;
//   for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
//     if (p->pid == pid) {
//       return p;
//     }
//   }
//   return 0; // Process not found
// }

int
sys_sort_syscalls(void)
{
  int pid;
  // struct proc *p;

  // Retrieve pid argument
  if (argint(0, &pid) < 0)
    return -1;

  // acquire(&ptable.lock);
  // Find the process with the given PID
  // p = find_proc(pid);
  // if (p == 0) {
  //   // release(&ptable.lock);
  //   cprintf("Process with PID %d not found.\n", pid);
  //   return -1;
  // }

  // Sort system calls for the process based on syscall number
  int a = sort_syscalls(pid);

  // release(&ptable.lock);

  return a;
}