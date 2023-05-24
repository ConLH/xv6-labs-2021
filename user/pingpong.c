#include "kernel/types.h"
#include "user/user.h"

#define READEND  0
#define WRITEEND 1

int main(int argc, char *argv[])
{
    if(argc != 1) {
        fprintf(2, "usage: pingpong\n");
        exit(1);
    }
    int p1[2];
    int p2[2];
    int pid;
    char buf[1];
    pipe(p1);
    pipe(p2);
    if((pid = fork()) < 0) {
        fprintf(2, "fork err!\n");
        exit(1);
    }else if(pid == 0) {
        close(p1[WRITEEND]);
        close(p2[READEND]);
        read(p1[READEND], buf, 1);
        printf("%d: received ping\n", getpid());
        write(p2[WRITEEND], "a", 1);
        close(p1[READEND]);
        close(p2[WRITEEND]);
        exit(0);
    }else {
        close(p1[READEND]);
        close(p2[WRITEEND]);
        write(p1[WRITEEND], "a", 1);
        read(p2[READEND], buf, 1);
        printf("%d: received pong\n", getpid());
        close(p1[WRITEEND]);
        close(p2[READEND]);
        exit(0);
    }
}
