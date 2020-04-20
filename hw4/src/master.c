#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <unistd.h>
/*
 * master
 * (See polya.h for specification.)
 */
void sigchld_handler(int sig){

}
int master(int workers) {
    sf_start();
    signal(SIGCHLD, sigchld_handler);
    int readfrommaster[2];
    int writetomaster[2];
    int pid;
    pipe(readfrommaster);
    pipe(writetomaster);
    for(int i=0; i<workers; i++){
        if((pid = fork()) == 0) {
            close(readfrommaster[1]);
            close(writetomaster[0]);
            dup2(readfrommaster[0], 0);
            dup2(writetomaster[1], 1);
        // Write to fd[1]
        }
        else {
            close(readfrommaster[0]);
            close(writetomaster[1]);

        // Read from fd[0]
        }
    }

    // TO BE IMPLEMENTED
    return EXIT_FAILURE;
}
