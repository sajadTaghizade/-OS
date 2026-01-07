#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "rwlock.h"
#include "sleeplock.h"

extern struct plock global_plock;
extern struct rwlock global_rwlock;
extern void sleeplock_test_run(void);
struct sleeplock testlock;

extern struct spinlock tickslock;
extern struct cpu cpus[NCPU];

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_simplearith(void)
{
  struct proc *p = myproc();

  int a = p->tf->ebx;
  int b = p->tf->ecx;

  int result = (a + b) * (a - b);

  cprintf("Calc: (%d+%d)*(%d-%d) = %d\n", a, b, a, b, result);

  return result;
}

int sys_show_process_family(void)
{
  int pid;

  if (argint(0, &pid) < 0)
  {
    return -1;
  }

  return show_process_family(pid);
}

int sys_start_throughput_measuring(void)
{
  return start_throughput_measuring();
}

int sys_end_throughput_measuring(void)
{
  return end_throughput_measuring();
}

int sys_print_process_info(void)
{
  print_process_info();
  return 0;
}

int sys_getlockstat(void)
{
  uint64 *score;
  uint64 kscore[NCPU];
  int i;

  if (argptr(0, (char **)&score, sizeof(uint64) * NCPU) < 0)
    return -1;

  for (i = 0; i < NCPU; i++)
  {
    if (tickslock.acq_count[i] > 0)
    {
      kscore[i] = (uint)tickslock.total_spins[i] / (uint)tickslock.acq_count[i];
    }
    else
    {
      kscore[i] = 0;
    }
  }

  for (i = 0; i < NCPU; i++)
  {
    score[i] = kscore[i];
  }

  return 0;
}

int sys_plock_acquire(void)
{
  int priority;
  if (argint(0, &priority) < 0)
    return -1;

  plock_acquire(&global_plock, priority);
  return 0;
}

int sys_plock_release(void)
{
  plock_release(&global_plock);
  return 0;
}

void ensure_testlock_init(void)
{
  if (testlock.name == 0)
    initsleeplock(&testlock, "test_lock");
}

int sys_test_acquire(void)
{
  ensure_testlock_init();
  acquiresleep(&testlock);
  return 0;
}

int sys_test_release(void)
{
  ensure_testlock_init();

  releasesleep(&testlock);
  return 0;
}

void ensure_rwlock_init(void)
{
  if (global_rwlock.name == 0)
    rwlock_init(&global_rwlock, "test_rwlock");
}

int sys_rwlock_read_acquire(void)
{
  ensure_rwlock_init();
  rwlock_acquire_read(&global_rwlock);
  return 0;
}

int sys_rwlock_read_release(void)
{
  ensure_rwlock_init();
  rwlock_release_read(&global_rwlock);
  return 0;
}

int sys_rwlock_write_acquire(void)
{
  ensure_rwlock_init();
  rwlock_acquire_write(&global_rwlock);
  return 0;
}

int sys_rwlock_write_release(void)
{
  ensure_rwlock_init();
  rwlock_release_write(&global_rwlock);
  return 0;
}

int sys_write_page(void)
{
  char *addr;
  int value;
  if (argptr(0, &addr, sizeof(char *)) < 0 || argint(1, &value) < 0)
    return -1;

  return handle_paging_request(addr, value, 1);
}

int sys_read_page(void)
{
  char *addr;
  if (argptr(0, &addr, sizeof(char *)) < 0)
    return -1;

  return handle_paging_request(addr, 0, 0);
}
