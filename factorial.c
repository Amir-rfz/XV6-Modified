#include "types.h"
#include "user.h"
#include "fcntl.h"

void acquire_user() {
  while ((open("lockfile", O_CREATE  | O_WRONLY)) < 0);
}

void release_user() {
  unlink("lockfile");
}

void test_shared_memory_with_factorial(int input_factorial) {
  int mem_id_num = 0; 
  int mem_id_fact = 1; 
  void *addr_num = (void *)open_shared_memory(mem_id_num); 
  void *addr_factorial = (void *)open_shared_memory(mem_id_fact); 

  for (int i = 0; i < input_factorial; i++) {
    int pid = fork();
    if (pid < 0) {
      printf(1, "fork failed\n");
      return;
    } 
    else if (pid == 0) {
      acquire_user();

      int pre_num = (*(int *)addr_num);
      int pre_fact = (*(int *)addr_factorial);
      (*(int *)addr_num)++;

      if (pre_fact == 0) {
        (*(int *)addr_factorial) = (pre_num + 1);
      }
      else {
        (*(int *)addr_factorial) = pre_fact * (pre_num + 1);
      }

      release_user();
      exit();
    }
  }

  for (int i = 0; i < input_factorial; i++) {
    wait();
  }

  printf(1, "Factorial of %d = %d\n", input_factorial, *(int *)addr_factorial);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf(1, "Usage: factorial <input_number>\n");
    exit();
  }

  int input_factorial = atoi(argv[1]);
  test_shared_memory_with_factorial(input_factorial);
  exit();
}
