#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[]) {

    int seconds;

    if(argc < 2){
        fprintf(2, "Usage: sleep <seconds>\n");
        exit(1);
    }
    // timer cycles about 1/10th second in qemu.
    seconds = 10*atoi(argv[1]);
    sleep(seconds);

    exit(0);
}