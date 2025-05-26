#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if (argc >= 2) {
        printf(2, "Usage: list_processes\n");
        exit();
    }
    if (list_all_processes() < 0) {
        printf(2, "Failed to list all processes\n");
    }
    exit();
}