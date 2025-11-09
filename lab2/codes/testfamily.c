#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid_child1, pid_child2, pid_grandchild1;
  int parent_pid = getpid();
  int pid_to_test;

  printf(1, "--- Test Program Starting (Parent PID: %d) ---\n", parent_pid);

  pid_child1 = fork();
  if(pid_child1 == 0){
    pid_grandchild1 = fork();
    if (pid_grandchild1 == 0) {
      printf(1, "Grandchild 1 (PID %d) created, sleeping...\n", getpid());
      sleep(300);
      exit();
    }
    printf(1, "Child 1 (PID %d) created, sleeping...\n", getpid());
    sleep(300);
    wait();
    exit();
  }

  pid_child2 = fork();
  if(pid_child2 == 0){
    printf(1, "Child 2 (PID %d) created, sleeping...\n", getpid());
    sleep(300);
    exit();
  }
  
  sleep(20); 
  
  if(argc < 2){
    printf(1, "\n--- No PID passed to test. ---\n");
    printf(1, "Created sleeping process family:\n");
    printf(1, "  Child 1 PID: %d\n", pid_child1);
    printf(1, "  Child 2 PID: %d\n", pid_child2);
    printf(1, "  (Grandchild was created by Child 1)\n");
    printf(1, "\nRun again with a PID to test, e.g.: testfamily %d\n", parent_pid);
  } else {
    pid_to_test = atoi(argv[1]);
    printf(1, "\n--- Calling show_process_family for PID %d ---\n", pid_to_test);

    show_process_family(pid_to_test);
    
    printf(1, "--- System call finished. ---\n");
  }
  wait();
  wait();
  
  printf(1, "--- Test Program Finished (Parent PID: %d) ---\n", parent_pid);
  exit();
}