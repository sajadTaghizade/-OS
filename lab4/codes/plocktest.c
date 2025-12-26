#include "types.h"
#include "stat.h"
#include "user.h"

void child_worker(int priority)
{
    printf(1, "Child %d requesting lock with priority %d\n", getpid(), priority);

    plock_acquire(priority);

    printf(1, "Child %d ACQUIRED lock with priority %d\n", getpid(), priority);

    sleep(100);

    plock_release();
    exit();
}

int main(int argc, char *argv[])
{
    printf(1, "Starting Test\n");

    int parent_prio = 10;
    printf(1, "Parent acquiring lock with Priority %d\n", parent_prio);
    plock_acquire(parent_prio);

    int priorities[] = {20, 50, 30, 40};
    int n = 4;

    for (int i = 0; i < n; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            child_worker(priorities[i]);
        }
        sleep(10);
    }

    sleep(100);

    plock_release();

    for (int i = 0; i < n; i++)
    {
        wait();
    }

    exit();
}