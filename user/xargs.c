#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"


int main(int argc, char *argv[]) {

    char *cmd, *buf, *args[MAXARG];
    int orgargc, pid, cur;

    if (argc < 2) {
        fprintf(2, "Usage: xargs <command> [args] ...");
        exit(1);
    }
    if (argc > MAXARG) {
        fprintf(2, "Error: too many args");
        exit(1);
    }
    cmd = argv[1];
    args[0] = cmd;
    for (orgargc = 1; orgargc < argc-1; orgargc++) {
        args[orgargc] = argv[orgargc+1];
    }
    cur = orgargc;
    buf = malloc(1024);
    args[cur] = buf;
    while (read(0, buf, 1))
    {
        if (*buf == ' ') {
            *buf = '\0';
            cur++;
            if (cur > MAXARG) {
                fprintf(2, "Error: too many args");
                exit(1);
            }
            buf = malloc(1024);
            args[cur] = buf;
            continue;
        }
        if (*buf == '\n') {
            *buf = '\0';

            pid = fork();
            if (pid == 0) {
                exec(cmd, args);
                exit(0);
            } else {
                for ( ; cur >= orgargc; cur--) {
                    free(args[cur]);
                }

                cur = orgargc;
                buf = malloc(1024);
                args[cur] = buf;
                wait(&pid);
                continue;
            }
        }
        buf++;
    }
    exit(0);
}