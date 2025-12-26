int sys_rwlock_test(void)
{
    int i;

    rwlock_init(&global_rwlock, "test rwlock"); 
    cprintf("RWLOCK TEST START\n");

    for (i = 0; i < 3; i++) {
        if (fork() == 0) {
            cprintf("reader %d trying to enter\n", myproc()->pid);


            rwlock_acquire_read(&global_rwlock);
            cprintf("reader %d ENTERED critical section\n", myproc()->pid);

            sleep(50);   

            cprintf("reader %d EXITING\n", myproc()->pid);
            rwlock_release_read(&global_rwlock);

            exit();
        }
    }

    sleep(10);

    if (fork() == 0) {
        cprintf("WRITER %d trying to enter\n", myproc()->pid);

        rwlock_acquire_write(&global_rwlock);
        cprintf("WRITER %d ENTERED critical section\n", myproc()->pid);

        sleep(50);

        cprintf("WRITER %d EXITING\n", myproc()->pid);
        rwlock_release_write(&global_rwlock);

        exit();
    }

    for (i = 0; i < 4; i++)
        wait();

    cprintf("RWLOCK TEST END\n");
    return 0;
}
