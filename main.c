#include <stdio.h>
#include <threads.h>

int run(void *arg);

int main(int argc, char **argv) {
    thrd_t thread;

    if (thrd_create(&thread, run, NULL) != thrd_success)
    {
        fprintf(stderr, "impossible the crée le thread!\n");
        return -1;
    }
    
    thrd_join(thread, NULL);

    printf("hello after thread code!\n");

    return 0;
}

int run(void *arg) {
    printf("hello from thread!\n");

    return 0;
}
