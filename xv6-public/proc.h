#define MAX_SYSCALLS 50
#define AGING_THRESHOLD 800
#define NUM_SHARED_MEMORY 64 

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
  int cpu_ticks;
  int syscall_count;
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct syscall_info {
    int number;         // System call number
    int count;          // Count of how many times this syscall was made
    const char* name;   // Name of syscall
};

enum schedule_queue {UNSET, ROUND_ROBIN, SJF, FCFS};

struct sjf_info {
  int arrival_time;
  int Confidence;
  int BurstTime;
};

struct schedule_info {
  enum schedule_queue queue;
  int last_run;
  struct sjf_info sjf;
  int arrival_queue_time;
  int get_cpu_time;
};

typedef struct SharedMemory {
  int mem_id;
  uint key;
  uint size;
  void *virtual_address;
} SharedMemory;

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  struct syscall_info syscall_data[MAX_SYSCALLS];
  int syscall_counts;
  int creation_time;
  int consecutive_time;
  struct schedule_info sched_info;
  SharedMemory pages[NUM_SHARED_MEMORY];
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
