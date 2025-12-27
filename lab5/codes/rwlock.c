#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "rwlock.h"


struct rwlock global_rwlock;

void rwlock_init(struct rwlock *rw, char *name)
{
  initlock(&rw->lk, "rwlock");
  rw->read_count = 0;
  rw->writer = 0;
  rw->name = name;
}



void rwlock_acquire_read(struct rwlock *rw)
{
  acquire(&rw->lk);

  while (rw->writer) {
    sleep(rw, &rw->lk);
  }

  rw->read_count++;

  release(&rw->lk);
}


void rwlock_release_read(struct rwlock *rw)
{
  acquire(&rw->lk);

  rw->read_count--;

  if (rw->read_count == 0) {
    wakeup(rw);
  }

  release(&rw->lk);
}


void rwlock_acquire_write(struct rwlock *rw)
{
  acquire(&rw->lk);

  while (rw->writer || rw->read_count > 0) {
    sleep(rw, &rw->lk);
  }

  rw->writer = 1;

  release(&rw->lk);
}


void rwlock_release_write(struct rwlock *rw)
{
  acquire(&rw->lk);

  rw->writer = 0;

  wakeup(rw);

  release(&rw->lk);
}
