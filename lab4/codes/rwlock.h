#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#include "spinlock.h"

struct rwlock {
  struct spinlock lk;   
  int read_count;      
  int writer;           
  char *name;  } ;       

#endif
