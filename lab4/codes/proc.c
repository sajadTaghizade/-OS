#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
extern uint ticks;

static void wakeup1(void *chan);

void push_back(struct cpu *c, struct proc *p)
{
  if (p->state != RUNNABLE)
    panic("push_back: process not runnable");

  p->next = 0;
  if (c->runq == 0)
  {
    c->runq = p;
  }
  else
  {
    struct proc *curr = c->runq;
    while (curr->next != 0)
      curr = curr->next;
    curr->next = p;
  }
}

struct proc *
pop_front(struct cpu *c)
{
  struct proc *p = c->runq;
  if (p)
  {
    c->runq = p->next;
    p->next = 0;
  }
  return p;
}

int cpu_get_load(struct cpu *c)
{
  int count = 0;
  struct proc *p = c->runq;
  while (p)
  {
    count++;
    p = p->next;
  }
  return count;
}

int is_movable(struct proc *p)
{
  if (p->pid == 1)
    return 0;
  if (p->pid == 2)
    return 0;

  if (p->name[0] == 's' && p->name[1] == 'h' && p->name[2] == 0)
    return 0;

  return 1;
}

void balance_load(void)
{
  struct cpu *c = mycpu();

  if (cpuid() % 2 != 0)
    return;

  acquire(&ptable.lock);

  int my_load = cpu_get_load(c);

  int min_load = 1000000;
  struct cpu *target_cpu = 0;
  int i;

  for (i = 0; i < ncpu; i++)
  {
    if (i % 2 != 0)
    {
      int load = cpu_get_load(&cpus[i]);
      if (load < min_load)
      {
        min_load = load;
        target_cpu = &cpus[i];
      }
    }
  }

  if (target_cpu == 0)
  {
    release(&ptable.lock);
    return;
  }

  if (my_load >= min_load + 3)
  {
    struct proc *prev = 0;
    struct proc *curr = c->runq;
    struct proc *victim = 0;

    while (curr)
    {
      if (is_movable(curr))
      {
        victim = curr;
        break;
      }
      prev = curr;
      curr = curr->next;
    }

    if (victim)
    {
      if (prev == 0)
      {
        c->runq = victim->next;
      }
      else
      {
        prev->next = victim->next;
      }
      victim->next = 0;

      victim->cpu_id = target_cpu - cpus;

      push_back(target_cpu, victim);

      // cprintf("LoadBalance: Moved PID %d from CPU %d (load %d) to CPU %d (load %d)\n",
              // victim->pid, cpuid(), my_load, target_cpu - cpus, min_load);
    }
  }

  release(&ptable.lock);
}

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  p->priority = 1;
  p->ticks_consumed = 0;
  p->ctime = ticks;
  p->finished_count = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// PAGEBREAK: 32
//  Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  int min_load = 1000000;
  struct cpu *best_cpu = &cpus[0];
  int i;

  for (i = 0; i < ncpu; i++)
  {
    if (i % 2 == 0)
    {
      int load = cpu_get_load(&cpus[i]);
      if (load < min_load)
      {
        min_load = load;
        best_cpu = &cpus[i];
      }
    }
  }
  p->cpu_id = best_cpu - cpus;
  // cprintf("userinit: PID %d assigned to E-core %d (Load: %d)\n", p->pid, best_cpu - cpus, min_load);
  push_back(best_cpu, p);

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  int min_load = 1000000;
  struct cpu *best_cpu = &cpus[0];
  // int i;

  for (i = 0; i < ncpu; i++)
  {
    if (i % 2 == 0)
    { 
      int load = cpu_get_load(&cpus[i]);
      if (load < min_load)
      {
        min_load = load;
        best_cpu = &cpus[i];
      }
    }
  }
  np->cpu_id = best_cpu - cpus;
  // cprintf("fork: PID %d assigned to E-core %d (Load: %d)\n", np->pid, best_cpu-cpus, min_load);
  push_back(best_cpu, np);

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        if(curproc->throughput_state == 1) {
            curproc->finished_count++;
        }
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); // DOC: wait-sleep
  }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.
//  void

void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  // int cpuid_val = c - cpus;
  c->proc = 0;

  // cprintf("CPU %d: Scheduler started (core_type=%d)\n", cpuid_val, c->core_type);

  for (;;)
  {
    sti();

    p = 0;

    acquire(&ptable.lock);

    if (cpuid() % 2 == 0)
    {
      p = pop_front(c);
      // if (p)
      // {
      //   cprintf("CPU %d (RR): Picked PID %d\n", cpuid_val, p->pid);
      // }
    }
    else
    {
      struct proc *min_p = 0;
      struct proc *curr = c->runq;
      struct proc *prev = 0;
      struct proc *min_prev = 0;

      // if (curr)
      // {
      //   cprintf("CPU %d (FCFS): Queue not empty, searching...\n", cpuid_val);
      // }

      while (curr)
      {
        if (min_p == 0 || curr->ctime < min_p->ctime)
        {
          min_p = curr;
          min_prev = prev;
        }
        prev = curr;
        curr = curr->next;
      }

      p = min_p;

      if (p)
      {
        // cprintf("CPU %d (FCFS): Found PID %d with ctime %d\n", cpuid_val, p->pid, p->ctime);

        if (min_prev == 0)
        {
          c->runq = p->next;
        }
        else
        {
          min_prev->next = p->next;
        }
        p->next = 0;
      }
    }

    if (p != 0)
    {
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->ticks_consumed = 0;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      c->proc = 0;
    }

    release(&ptable.lock);

    if (p == 0)
    {
      sti();
      hlt();
    }
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); // DOC: yieldlock
  myproc()->state = RUNNABLE;
  push_back(mycpu(), myproc());
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        // DOC: sleeplock0
    acquire(&ptable.lock); // DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      // Assign to current CPU or original CPU?
      // Simple strategy: push to current CPU's queue or re-balance
      // For now, push to the CPU determined by PID to keep it simple/consistent
      int cpu_idx = p->pid % ncpu;
      push_back(&cpus[cpu_idx], p);
    }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
      {
        p->state = RUNNABLE;
        int cpu_idx = p->pid % ncpu;
        push_back(&cpus[cpu_idx], p);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int show_process_family(int pid)
{
  struct proc *p;
  struct proc *target_proc = 0;
  struct proc *parent_proc = 0;
  int parent_pid = -1;
  int found_children = 0;
  int found_siblings = 0;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid && p->state != UNUSED)
    {
      target_proc = p;
      if (p->parent)
      {
        parent_proc = p->parent;
        parent_pid = p->parent->pid;
      }
      break;
    }
  }

  if (target_proc == 0)
  {
    release(&ptable.lock);
    cprintf("PID is not valid\n");
    return -1;
  }

  if (target_proc->pid == 1)
  {
    cprintf("My id: %d, My parent id: %d (This is the init process)\n", target_proc->pid, target_proc->pid);
  }
  else
  {
    cprintf("My id: %d, My parent id: %d\n", target_proc->pid, parent_pid);
  }

  cprintf("Children of process %d:\n", pid);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == target_proc && p->state != UNUSED)
    {
      cprintf("Child pid: %d\n", p->pid);
      found_children = 1;
    }
  }
  if (!found_children)
  {
    cprintf("(No children found)\n");
  }

  cprintf("Siblings of process %d:\n", pid);
  if (parent_proc)
  {
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent == parent_proc && p->pid != pid && p->state != UNUSED)
      {
        cprintf("Sibling pid: %d\n", p->pid);
        found_siblings = 1;
      }
    }
  }
  if (!found_siblings)
  {
    cprintf("(No siblings found)\n");
  }

  release(&ptable.lock);
  return 0;
}

int sys_set_priority_syscall(void)
{
  int pid, priority;
  struct proc *p;

  if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->priority = priority;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);

  cprintf("set_priority: PID %d not found\n", pid);
  return -1;
}


int
start_throughput_measuring(void)
{
  struct proc *p = myproc();
  p->start_ticks = ticks;
  p->throughput_state = 1;
  p->finished_count = 0;
  cprintf("Throughput measurement started at tick %d (PID %d)\n", p->start_ticks, p->pid);
  return 0;
}

int
end_throughput_measuring(void)
{
struct proc *p = myproc();

  if (p->throughput_state == 0) {
    cprintf("Error: Throughput measurement not started.\n");
    return -1;
  }
  
  p->throughput_state = 0;
  int end_ticks = ticks;
  int elapsed_ticks = end_ticks - p->start_ticks;
  
  if (elapsed_ticks <= 0) {
    cprintf("Error: Elapsed time is zero or negative.\n");
    return -1;
  }
  
  int completed_procs = p->finished_count;

int throughput_scaled = (completed_procs * 1000) / elapsed_ticks;
  int whole_part = throughput_scaled / 1000;
  int decimal_part = throughput_scaled % 1000;

  cprintf("\n--- Throughput Measurement Results (PID %d) ---\n", p->pid);
  cprintf("Total completed processes: %d\n", completed_procs);
  cprintf("Elapsed time (ticks): %d\n", elapsed_ticks);
  
  cprintf("Throughput: %d.", whole_part);

  if (decimal_part < 10) {
    cprintf("00");
  } else if (decimal_part < 100) {
    cprintf("0");
  }
  
  cprintf("%d Processes/Tick\n", decimal_part);
  // -----------------------------
  
  cprintf("--------------------------------------\n\n");
  
  return 0;
}

void
print_process_info(void)
{
  struct proc *p = myproc();

  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "runing",
  [ZOMBIE]    "zombie"
  };

  cprintf("\nPID \t State \t \t Algo \t Life \t CPU\n");
  cprintf("----------------------------------------------------\n");

  if (p != 0 && p->state != UNUSED) {
      
      int lifetime = ticks - p->ctime;

      char *algo = (p->cpu_id % 2 == 0) ? "RR" : "FCFS";

      cprintf("%d \t %s \t %s \t %d \t %d\n", 
        p->pid, 
        states[p->state],
        algo, 
        lifetime,
        p->cpu_id
      );
  }

  cprintf("----------------------------------------------------\n\n");
}