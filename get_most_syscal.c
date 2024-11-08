#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf(2, "Usage: get_most_syscall <pid>\n");
        exit();
    }
    int pid = atoi(argv[1]);
    if (get_most_invoked_syscall(pid) < 0) {
        printf(2, "Failed to get most invoked syscall for PID %d\n", pid);
    } else {
        exit();
    }
    exit();
}


