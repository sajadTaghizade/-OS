#include "types.h"
#include "stat.h"
#include "user.h"

void spin() {
    volatile int i;
    for(i=0; i<100000000; i++); // کار سنگین و طولانی
}

int main() {
    int i;
    int n = 20; // تعداد زیاد پروسه

    printf(1, "Starting Load Balance Test...\n");

    // 1. ایجاد 20 پروسه
    // طبق قانون جدید، همه این‌ها باید اول بروند توی صف هسته 0 (چون زوج است)
    for(i=0; i<n; i++) {
        int pid = fork();
        if(pid == 0) {
            spin();
            printf(1, "Child %d finished on CPU ???\n", getpid());
            exit();
        }
    }

    // 2. صبر میکنیم. در این مدت هسته 0 باید بفهمد که شلوغ است
    // و تعدادی را به هسته 1 (فرد) هل بدهد.
    
    for(i=0; i<n; i++) wait();
    
    printf(1, "All finished.\n");
    exit();
}