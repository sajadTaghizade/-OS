#include "types.h"
#include "stat.h"
#include "user.h"

void spin(int iterations) {
  volatile int i;
  for (i = 0; i < iterations * 1000000; i++) {
    ;
  }
}

int main(int argc, char *argv[]) {
  int pid;
  int i;

  pid = fork();

  if (pid < 0) {
    printf(1, "fork failed\n");
    exit();
  }

  for (i = 0; i < 10; i++) {
    spin(100); 
    printf(1, "pid %d: running...\n", getpid());
  }

  if (pid > 0) {
    wait();
  }

  exit();
}
