#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

const int KEY = 1; 

void CalculateDecode(const char *word1, char *result) {
    int i = 0;
    while (word1[i] != '\0') {
        char c = word1[i];
        // Check if the character is lowercase
        if (c >= 'a' && c <= 'z') {
            result[i] = ((c - 'a' - KEY + 26) % 26) + 'a';
        }
        // Check if the character is uppercase
        else if (c >= 'A' && c <= 'Z') {
            result[i] = ((c - 'A' - KEY + 26) % 26) + 'A';
        }
        // Keep other characters unchanged
        else {
            result[i] = c;
        }
        i++;
    }
    result[i] = '\0';  // Null-terminate the result string
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf(1, "Usage: decode <text_to_encode>\n");
        exit();
    }

    int fd = open("result.txt", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(1, "Can't create or open the file\n");
        exit();
    }

    // Allocate buffer dynamically or use a static buffer
    char result[512]; 

    // Decode and write each argument (argv[1] to argv[argc-1]) to the file
    for (int i = 1; i < argc; i++) {
        CalculateDecode(argv[i], result);
        write(fd, result, strlen(result));

        // Add a space between words
        if (i < argc - 1) {
            write(fd, " ", 1);
        }
    }

    char diff[2];
    diff[0] = '\n';
    diff[1] = '\0';
    write(fd, diff, 1);
    
    close(fd);
    exit();
}