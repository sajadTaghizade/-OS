// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
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