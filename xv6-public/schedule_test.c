#include "types.h"
#include "user.h"

#define PROCESSS_NUM 5

int main()
{
    for (int i = 0; i < PROCESSS_NUM; i++)
    {
        int pid = fork();
        if (pid == 0) {
            for (int j = 0; j < 1000; j++) {
                for (int k = 0; k < 1000; k++) {
                    getpid();
                }
            }
            exit();
        }
    }
    for (int i = 0; i < PROCESSS_NUM; i++)
        wait();
    exit(); 
}
