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

  // 1. Create Child 1
  pid_child1 = fork();
  if(pid_child1 == 0){
    // --- This is Child 1 ---
    pid_grandchild1 = fork();
    if (pid_grandchild1 == 0) {
      // --- This is Grandchild 1 ---
      printf(1, "Grandchild 1 (PID %d) created, sleeping...\n", getpid());
      sleep(300); // Sleep for a long time
      exit();
    }
    // --- Back in Child 1 ---
    printf(1, "Child 1 (PID %d) created, sleeping...\n", getpid());
    sleep(300); // Sleep for a long time
    wait();     // Wait for Grandchild 1
    exit();
  }

  // --- Back in Parent ---
  
  // 2. Create Child 2
  pid_child2 = fork();
  if(pid_child2 == 0){
    // --- This is Child 2 ---
    printf(1, "Child 2 (PID %d) created, sleeping...\n", getpid());
    sleep(300); // Sleep for a long time
    exit();     // Then exit
  }

  // --- Back in Parent ---
  // Give all children/grandchildren time to be created and print their PIDs
  sleep(20); 

  // 3. Check if user passed a PID to test
  if(argc < 2){
    // --- NO PID PASSED ---
    // Print the list of created processes so the user knows what to test
    printf(1, "\n--- No PID passed to test. ---\n");
    printf(1, "Created sleeping process family:\n");
    printf(1, "  Child 1 PID: %d\n", pid_child1);
    printf(1, "  Child 2 PID: %d\n", pid_child2);
    printf(1, "  (Grandchild was created by Child 1)\n");
    printf(1, "\nRun again with a PID to test, e.g.: testfamily %d\n", parent_pid);
  } else {
    // --- PID WAS PASSED ---
    pid_to_test = atoi(argv[1]);
    printf(1, "\n--- Calling show_process_family for PID %d ---\n", pid_to_test);
    
    // Call the system call with the provided PID
    show_process_family(pid_to_test);
    
    printf(1, "--- System call finished. ---\n");
  }

  // Parent waits for its direct children to exit
  wait();
  wait();
  
  printf(1, "--- Test Program Finished (Parent PID: %d) ---\n", parent_pid);
  exit();




    // int  pid_to_test = atoi(argv[1]);

    // // Call the system call with the provided PID
    // show_process_family(pid_to_test);
    // exit();
}