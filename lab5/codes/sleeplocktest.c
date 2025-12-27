#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
  printf(1, "Starting Sleeplock\n");

  printf(1, "Parent Acquiring lock\n");
  test_acquire();
  printf(1, "Parent Lock acquired.\n");

  int pid = fork();

  if (pid < 0)
  {
    printf(1, "Fork failed\n");
    exit();
  }

  if (pid == 0)
  {
    printf(1, "Child: Trying to release parent lock (PANIC now)\n");

    test_release();

    printf(1, "ERROR Child survived! Protection failed.\n");
    exit();
  }
  else
  {
    wait();
    printf(1, "Parent Releasing lock normally.\n");
    test_release();
  }

  exit();
}