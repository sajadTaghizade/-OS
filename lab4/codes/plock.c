#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct plock global_plock;

void plock_init(struct plock *pl, char *name)
{
    initlock(&pl->lk, "plock_internal");
    pl->name = name;
    pl->locked = 0;
    pl->head = 0;
}

void plock_acquire(struct plock *pl, int priority)
{
    acquire(&pl->lk);

    if (pl->locked == 0)
    {
        pl->locked = 1;
        release(&pl->lk);
    }
    else
    {
        struct plock_node *node = (struct plock_node *)kalloc();
        node->proc = myproc();
        node->priority = priority;
        node->next = pl->head;
        pl->head = node;
        sleep(node, &pl->lk);
        kfree((char *)node);
        release(&pl->lk);
    }
}
static struct plock_node *plock_pop_max(struct plock *pl)
{
    if (pl->head == 0)
        return 0;

    struct plock_node *curr = pl->head;
    struct plock_node *max_node = curr;
    struct plock_node *prev = 0;
    struct plock_node *prev_of_max = 0;

    while (curr != 0)
    {
        if (curr->priority > max_node->priority)
        {
            max_node = curr;
            prev_of_max = prev;
        }
        prev = curr;
        curr = curr->next;
    }

    if (prev_of_max == 0)
    {
        pl->head = max_node->next;
    }
    else
    {
        prev_of_max->next = max_node->next;
    }

    max_node->next = 0;

    return max_node;
}

void plock_release(struct plock *pl)
{
    acquire(&pl->lk);

    struct plock_node *winner = plock_pop_max(pl);

    if (winner != 0)
    {
        wakeup(winner);
    }
    else
    {
        pl->locked = 0;
    }

    release(&pl->lk);
}