#include "types.h"
#include "user.h"
#include "stat.h"

#define NCHILD 4
#define NCPU 8

void
work()
{
  int i;
  
  for(i = 0; i < 10000; i++){
    uptime(); 
  }
}

int
main(int argc, char *argv[])
{
  uint64 scores[NCPU];
  int i, pid;

  printf(1, "Starting lock profiling test on tickslock...\n");

  getlockstat(scores);
  printf(1, "Initial scores:\n");
  for(i = 0; i < NCPU; i++)
    printf(1, "CPU %d: %d\n", i, (int)scores[i]);

  for(i = 0; i < NCHILD; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "Fork failed\n");
      exit();
    }
    if(pid == 0){
      work();
      exit();
    }
  }

  for(i = 0; i < NCHILD; i++){
    wait();
  }

  getlockstat(scores);
  printf(1, "\nFinal scores (Contention Metrics):\n");
  for(i = 0; i < NCPU; i++){
    printf(1, "CPU %d: %d\n", i, (int)scores[i]);
  }

  exit();
}