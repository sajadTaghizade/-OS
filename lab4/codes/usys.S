#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(make_duplicate)
SYSCALL(show_process_family) 
SYSCALL(grep_syscall)
SYSCALL(start_throughput_measuring)
SYSCALL(end_throughput_measuring)
SYSCALL(print_process_info)
SYSCALL(getlockstat)


SYSCALL(set_priority_syscall)
.globl simple_arithmetic_syscall
simple_arithmetic_syscall:
    pushl %ebx
    movl 8(%esp), %ebx
    movl 12(%esp), %ecx
    movl $SYS_simplearith, %eax
    int $T_SYSCALL
    popl %ebx
    ret
