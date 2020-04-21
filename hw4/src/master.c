#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <unistd.h>
#include <sys/wait.h>
/*
 * master
 * (See polya.h for specification.)
 */
static volatile sig_atomic_t current=-1;
static volatile sig_atomic_t done=0;
static volatile sig_atomic_t count=0;
void sigchld_handler(int sig){
    current=waitpid(-1, NULL, WNOHANG);
}
int master(int workers) {
    sf_start();
    signal(SIGCHLD, sigchld_handler);
    int readfrommaster[workers][2];
    int writetomaster[workers][2];
    pid_t pid[workers];
    int state[workers];
    for(int i=0; i<workers; i++){
        pipe(readfrommaster[i]);
        pipe(writetomaster[i]);
        if((pid[i] = fork()) == 0) {
            close(readfrommaster[i][1]);
            close(writetomaster[i][0]);
            dup2(readfrommaster[i][0], 0);
            dup2(writetomaster[i][1], 1);
            state[i]=1;
        }
    }
    while(!done){
        while(current==-1);
        for(int j=0; j<workers; j++){
            if(pid[j]==current){
                if(state[j]==1){
                    state[j]=2;
                    kill(current, SIGCONT);
                    state[j]=3;
                    break;
                }
                else if(state[j]==3){
                    state[j]=4;
                    close(readfrommaster[j][0]);
                    close(writetomaster[j][1]);
                    struct problem *problems=get_problem_variant(workers, j);
                    write(readfrommaster[j][1], problems, problems->size);
                    break;
                }
                else if(state[j]==4){
                    state[j]=5;
                    struct result *results=malloc(sizeof(struct result));
                    read(writetomaster[j][0], results, sizeof(struct result));
                    size_t data=results->size-sizeof(struct result);
                    results=realloc(results, results->size);
                    read(writetomaster[j][0], (void *)results+sizeof(struct result), data);
                    break;
                }
            }
        }
        current=-1;
    }

    // while(!cont) {
    //     close(readfrommaster[0]);
    //     close(writetomaster[1]);
    //     struct problem *problems=get_problem_variant(workers, i);
    //     write(readfrommaster[1], problems, problems->size);
    //     struct result *results=malloc(sizeof(struct result));
    //     read(writetomaster[0], results, sizeof(struct result));
    //     size_t data=results->size-sizeof(struct result);
    //     results=realloc(results, results->size);
    //     read(writetomaster[0], (void *)results+sizeof(struct result), data);
    //     }
    // Read from fd[0]
    }

    // TO BE IMPLEMENTED
    return EXIT_FAILURE;
}
