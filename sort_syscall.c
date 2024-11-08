#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
  if (argc < 2) {
    printf(2, "Usage: sort_syscall <pid>\n");
    exit();
  }

  int pid = atoi(argv[1]);
  if (sort_syscalls(pid) < 0) {
    printf(2, "Failed to sort syscalls for PID %d\n", pid);
  } else {
    printf(1, "Syscalls sorted for PID %d\n", pid);
  }

  exit();
}
