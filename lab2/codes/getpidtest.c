#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
    printf(1, "Start\n");
    int pid = getpid();
    printf(1, "PID is: %d\n", pid);
    printf(1, "finished.\n");
    exit();
}