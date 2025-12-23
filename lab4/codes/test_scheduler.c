#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_CHILDREN 5

void 
heavy_loop(char id, int iterations)
{
  int i, j;
  volatile int x = 0; 
  printf(1, "\n");
  
  for (i = 0; i < iterations; i++)
  {
    for (j = 0; j < 2500000; j++)
    {
      x += 1;
    }
    // print_process_info(); 

    for (j = 0; j < 2500000; j++)
    {
      x += 1;
    }
    // print_process_info(); 

    for (j = 0; j < 2500000; j++)
    {
      x += 1;
    }
    print_process_info(); 

    for (j = 0; j < 2500000; j++)
    {
      x += 1;
    }
    
    // printf(1, "\nid is :%c\n", id); 
  }
  printf(1, "\n");
}

int 
main(int argc, char *argv[])
{
  int i;
  int pid;
  int loop_iterations = 2;
  
  printf(1, "\n--- Starting Scheduler Evaluation ---\n");
  printf(1, "Creating %d CPU-bound children, each running %d heavy iterations.\n", NUM_CHILDREN, loop_iterations);

  printf(1, "\n[INFO] State before workload creation:\n");
  print_process_info(); 
  
  printf(1, "\n[MEASUREMENT] Starting throughput measurement...\n");
  start_throughput_measuring(); 


  for (i = 0; i < NUM_CHILDREN; i++)
  {
    char id = '1' + i; 
    
    pid = fork();
    
    if (pid < 0)
    {
      printf(1, "fork failed for child %c\n", id);
      break; 
    }

    if (pid == 0)
    {
      heavy_loop(id, loop_iterations);
      printf(1, " (Child %c PID %d finished) ", id, getpid());
      exit();
    }
  }

  for (i = 0; i < NUM_CHILDREN; i++)
  {
    wait();
  }
  
  printf(1, "\n\n[INFO] State after all children finished (expect ZOMBIEs):\n");
  print_process_info(); 

  printf(1, "\n[MEASUREMENT] Ending measurement and calculating throughput:\n");
  end_throughput_measuring(); 
  
  printf(1, "\n--- Scheduler Test Finished ---\n");
  exit();
}