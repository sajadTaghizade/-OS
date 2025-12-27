#ifndef SPINLOCK_H 
#define SPINLOCK_H
#include "types.h"
#include "param.h"
// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.

  uint64 acq_count[NCPU];   // Times lock acquired on each CPU
  uint64 total_spins[NCPU]; // Total spin cycles on each CPU
};

struct plock_node {
  struct proc *proc;       
  int priority;            
  struct plock_node *next; 
};

struct plock {
  struct spinlock lk;      
  int locked;  
  struct plock_node *head; 
  char *name;              
};

#endif