#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "rwlock.h"

extern struct plock global_plock;
extern struct rwlock global_rwlock;
extern void sleeplock_test_run(void);


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


int
sys_sleeplock_test(void)
{
  sleeplock_test_run();
  return 0;
}
