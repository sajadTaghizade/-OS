#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#include "spinlock.h"

struct rwlock {
  struct spinlock lk;   // محافظ داخلی
  int read_count;       // تعداد خوانندگان فعال
  int writer;           // آیا نویسنده داخل است؟
  char *name;           // برای debug
};

#endif
