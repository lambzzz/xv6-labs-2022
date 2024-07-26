#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[]) {

    char byte = '0';
    int pid, pipefd_ch[2], pipefd_pa[2];
    // pipefd[0]:read, pipefd[1]:write
    pipe(pipefd_ch);
    pipe(pipefd_pa);
    pid = fork();

    if (pid == 0) {
        // child
        read(pipefd_pa[0], &byte, 1);
        printf("%d: received ping\n", getpid());
        write(pipefd_ch[1], &byte, 1);
        close(pipefd_pa[0]);
        close(pipefd_pa[1]);
    } else {
        // parent
        write(pipefd_pa[1], &byte, 1);
        read(pipefd_ch[0], &byte, 1);
        printf("%d: received pong\n", getpid());
        close(pipefd_ch[0]);
        close(pipefd_ch[1]);
    }
    
    exit(0);
}