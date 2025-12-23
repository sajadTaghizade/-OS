#include "types.h"
#include "stat.h"
#include "user.h"

void spin() {
    volatile int i;
    for(i=0; i<100000000; i++);
}

int main() {
    int i;
    int n = 20;

    printf(1, "Starting Load Balance Test...\n");

    for(i=0; i<n; i++) {
        int pid = fork();
        if(pid == 0) {
            spin();
            printf(1, "Child %d finished\n", getpid());
            exit();
        }
    }

    for(i=0; i<n; i++) wait();
    
    printf(1, "All finished.\n");
    print_process_info();
    exit();
}