#include "types.h"
#include "stat.h"
#include "user.h"

void worker(int id)
{
  volatile int x = 0;
  int j;
  for (j = 0; j < 5000000; j++)
  {
    x += j;
  }
  printf(1, "Child %d finished safely.\n", id);
  exit();
}

int main()
{
  int i;
  int n_children = 20;

  printf(1, "Starting Queue Stress Test with %d children...\n", n_children);

  for (i = 0; i < n_children; i++)
  {
    int pid = fork();
    if (pid == 0)
    {
      worker(i);
    }
  }

  for (i = 0; i < n_children; i++)
  {
    wait();
  }

  printf(1, "TEST PASSED: All children finished without Kernel Panic.\n");
  exit();
}