#include "types.h"
#include "stat.h"
#include "user.h"
int main() {
    printf(1, "Testing recursive functionality of reentrant lock...\n");
    if (testreentrantlock() == 0) {
        printf(1, "Recursive test passed!\n");
    } else {
        printf(1, "Recursive test failed!\n");
    }
    exit();
}