#include "kernel/types.h"
#include "user/user.h"

#define READEND     0
#define WRITEEND    1

void child(int* pl);

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    if(fork() == 0) {
        child(p);
    } else {
        close(p[READEND]);
        for(int i = 2; i <= 35; ++i) {
            write(p[WRITEEND], &i, sizeof(int));
        }
        close(p[WRITEEND]);
        wait((int *) 0);
    }
    exit(0);
}

void child(int* pl) {
    int pr[2];
    int n;

    close(pl[WRITEEND]);
    if(read(pl[READEND], &n, sizeof(int)) == 0) {
        exit(0);
    }
    pipe(pr);
    if(fork() == 0) {
        child(pr);
    }else {
        int prime = n;
        printf("prime %d\n", prime);
        close(pr[READEND]);
        while(read(pl[READEND], &n, sizeof(int)) != 0) {
            if(n % prime != 0) {
                write(pr[WRITEEND], &n, sizeof(int));
            }
        }
        close(pr[WRITEEND]);
        wait((int *) 0);
    }
    exit(0);
}
