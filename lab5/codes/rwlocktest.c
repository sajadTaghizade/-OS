#include "types.h"
#include "stat.h"
#include "user.h"

void reader(int id)
{
    printf(1, "Reader %d: trying to enter\n", id);

    rwlock_read_acquire();

    printf(1, "Reader %d: ENTERED (Reading)\n", id);
    sleep(50);

    printf(1, "Reader %d: Exiting\n", id);
    rwlock_read_release();

    exit();
}

void writer(int id)
{
    printf(1, "\nWriter %d: trying to enter\n", id);

    rwlock_write_acquire();

    printf(1, "--- Writer %d: ENTERED (Writing...)\n", id);
    sleep(50);

    printf(1, "--- Writer %d: Exiting \n", id);
    rwlock_write_release();

    exit();
}

int main(void)
{
    printf(1, "Starting RWLock Test\n");

    for (int i = 0; i < 3; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            reader(i);
        }
    }

    sleep(10);

    int pid = fork();
    if (pid == 0)
    {
        writer(100);
    }

    for (int i = 0; i < 4; i++)
    {
        wait();
    }

    printf(1, "RWLock Test Finished.\n");
    exit();
}