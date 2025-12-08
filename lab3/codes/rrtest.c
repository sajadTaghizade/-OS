#include "types.h"
#include "stat.h"
#include "user.h"

void heavy_loop(char id)
{
  int i, j;
  volatile int x = 0;
  for (i = 0; i < 500; i++)
  {
    for (j = 0; j < 1000000; j++)
    {
      x += 1;
    }

    printf(1, "%c ", id);
  }
}

int main(int argc, char *argv[])
{
  int pid = fork();

  if (pid < 0)
  {
    printf(1, "fork failed\n");
    exit();
  }

  if (pid == 0)
  {
    heavy_loop('B');
    exit();
  }
  else
  {
    heavy_loop('A');
    wait();
  }

  printf(1, "\nRR Test Finished\n");
  exit();
}