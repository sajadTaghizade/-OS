#include "types.h"
#include "stat.h"
#include "user.h"



void cpu_intensive_task() {
  volatile unsigned long long i;
  for(i = 0; i < 500000000; i++) {
     asm("nop"); 
  }
}

int
main(void)
{
  int pid1, pid2;

  printf(1, "Starting\n");

  pid1 = fork();
  if(pid1 == 0) {
    printf(1, "PID %d Child 1 (High Priority) starting task...\n", getpid());
    cpu_intensive_task();
    printf(1, "[PID %d] === Child 1 (High Priority) FINISHED ===\n", getpid()); 
    exit();
  }

  pid2 = fork(); 
  if(pid2 == 0) {
    printf(1, "[PID %d] Child 2 (Low Priority) starting task...\n", getpid());
    cpu_intensive_task();
    printf(1, "[PID %d] === Child 2 (Low Priority) FINISHED ===\n", getpid()); 
    exit();
  }
  
  printf(1, "Parent: Setting priorities (High=%d, Low=%d)\n", pid1, pid2);
  
  set_priority_syscall(pid1, 0);
  set_priority_syscall(pid2, 2); 

  printf(1, "Parent: Waiting for children to finish.\n");
  
  wait();
  wait();
  
  printf(1, "Parent: Both children finished. Test complete.\n");
  exit();
}