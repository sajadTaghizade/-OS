#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"

struct sleeplock testlock;

void
sleeplock_test_run(void)
{
  cprintf("SLEEPLOCK TEST START\n");

  initsleeplock(&testlock, "test sleep lock");

  acquiresleep(&testlock);
  cprintf("parent acquired sleeplock\n");

  if (fork() == 0) {
    cprintf("child trying to release sleeplock (should panic)\n");
    releasesleep(&testlock); 
    exit();
  }

  wait();
}
