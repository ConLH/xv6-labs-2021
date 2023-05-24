#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if(argc < 2) {
        fprintf(2, "usage: xargs [command] [argvs]");
        exit(1);
    }
    char arg[1024];
    char *args[MAXARG];
    int pc = 0, ps = 0;
    char n;
    while(read(0, &n, 1) != 0) {
        if(n == ' ') {
            arg[++pc] = '\0'; 
            args[ps++] = arg;
            pc = 0;
        }else if(n == '\n') {
            arg[++pc] = '\0';
            args[ps++] = arg;
            pc = 0;
            int pid = fork();
            if(pid < 0) {
                fprintf(2, "fork error\n");
                exit(1);
            }else if (pid == 0) {
                char* argtoe[MAXARG];
                int pos = 0;
                for(int i = 1; i < argc; i++, pos++) {
                    argtoe[pos] = argv[i];
                }
                for(int i = 0; i <= ps; i++, pos++) {
                    argtoe[pos] = args[i];
                }
                exec(argv[1], argtoe);
            }else {
                wait((int*) 0);
            }
            ps = 0;
        }else {
            arg[pc++] = n;
        }
    }
    exit(0);
}
