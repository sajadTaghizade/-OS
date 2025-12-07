#include "types.h"
#include "stat.h"
#include "user.h"

void heavy_task(int id) {
    volatile int i;
    printf(1, "Proc %d STARTED\n", id);
    
    for (i = 0; i < 400000000; i++) { 
        ; 
    }
    
    printf(1, "Proc %d FINISHED\n", id);
}

int main() {
    int i;
    for (i = 10; i < 14; i++) {
        int pid = fork();
        if (pid == 0) {
            heavy_task(getpid());
            exit();
        }
    }

    for (i = 0; i < 4; i++) wait();
    exit();
}