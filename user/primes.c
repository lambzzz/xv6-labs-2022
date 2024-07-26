#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[]) {

    int i, prime, data[34], buf, len = 34;
    int pid, pipefd[2], endsig = -1;
    // pipefd[0]:read, pipefd[1]:write
    for (i = 0; i < 34; i++)
    {
        data[i] = 2 + i;
    }
    while (len > 0) {
        pipe(pipefd);
        pid = fork();
        if (pid == 0) {
            // child
            len = 0;
            read(pipefd[0], &prime, 4);
            printf("prime %d\n", prime);
            while(1) {
                read(pipefd[0], &buf, 4);
                if (buf == -1)
                    break;
                if (buf % prime == 0)
                    continue;
                data[len] = buf;
                len++;
            }
            close(pipefd[0]);
            close(pipefd[1]);
        } else {
            // parent
            for (i = 0; i < len; i++) {
                write(pipefd[1], &data[i], 4);
            }
            write(pipefd[1], &endsig, 4);
            wait(&pid);
            exit(0);
        }
    }
    
    exit(0);
}