#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"


extern struct plock global_plock;


int sys_plock_test(void)
{
    cprintf("plock test started\n");


    plock_acquire(&global_plock, 10);
    cprintf("parent acquired plock\n");

    int pid = fork();

    if (pid == 0) {
       
        cprintf("child trying to release plock (should panic)\n");

        plock_release(&global_plock);

        cprintf("ERROR: child released plock!\n");
        exit();
    }

    wait();

    cprintf("ERROR: system did not panic\n");

    plock_release(&global_plock);
    return 0;
}

