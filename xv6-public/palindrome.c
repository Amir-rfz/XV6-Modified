#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        if (argc < 2) {
            printf(2, "Error: You didn't enter the number!\n");
        } 
		else {
            printf(2, "Error: Too many arguments!\n");
        }
        exit();
    }

    int last_ebx_value;
    int number = atoi(argv[1]);

    asm volatile(
        "movl %%ebx, %0;"        
        "movl %1, %%ebx;"        
        : "=r" (last_ebx_value)  
        : "r" (number)           
    );

    printf(1, "User: Requesting palindrome creation for the input number: %d\n", number);
    create_palindrome();

    asm volatile("movl %0, %%ebx" : : "r" (last_ebx_value));  

    exit();
}
