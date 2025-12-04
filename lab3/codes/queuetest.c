#include "types.h"
#include "stat.h"
#include "user.h"

void work(int pid) {
  volatile int i;
  int j;
  for (j = 0; j < 5; j++) {
    for (i = 0; i < 10000000; i++) {
      ; 
    }
  }
}

int main(int argc, char *argv[]) {
  int i;
  int pid;
  int n_children = 5;

  printf(1, "starting %d children\n", n_children);

  for (i = 0; i < n_children; i++) {
    pid = fork();
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit();
    }
    if (pid == 0) {
      int mypid = getpid();
      printf(1, "child %d started\n", mypid);
      work(mypid);
      printf(1, "child %d finished\n", mypid);
      exit();
    }
  }

  // 
  for (i = 0; i < n_children; i++) {
    wait();
  }

  printf(1, "all children finished\n");
  exit();
}
