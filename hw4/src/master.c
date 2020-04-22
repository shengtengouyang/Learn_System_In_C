#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <unistd.h>
#include <sys/wait.h>
/*
 * master
 * (See polya.h for specification.)
 */
static volatile sig_atomic_t states[MAX_WORKERS];
static volatile pid_t pids[MAX_WORKERS];
int findIndex(pid_t pid){
    for(int i=0; i<MAX_WORKERS; i++){
        if(pids[i]==pid){
            return i;
        }
    }
    return -1;
}
void sigchld_handler(int sig){
    debug("meet sigchld");
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    int status;
    pid_t current;
    while((current=waitpid(-1, &status, WUNTRACED|WCONTINUED|WNOHANG)!=0)){
        int index=findIndex(current);
        debug("print current %d", current);
        if(index==-1){
            return;
        }
        if(WIFEXITED(status)){
            states[index]=5;
        }
        else if(WIFCONTINUED(status)){
            continue;
        }
        else if(WIFSTOPPED(status)){
            states[index]++;
        }
        else{
            states[index]=6;
        }
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);
    debug("current is %d", current);
}


int master(int workers) {
    sf_start();
    signal(SIGCHLD, sigchld_handler);
    int readfrommaster[workers][2];
    int writetomaster[workers][2];
    struct problem  *problems [workers];
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    for(int i=0; i<workers; i++){
        pipe(readfrommaster[i]);
        pipe(writetomaster[i]);
        if((pids[i] = fork()) == 0) {
            close(readfrommaster[i][1]);
            close(writetomaster[i][0]);
            dup2(readfrommaster[i][0], 0);
            dup2(writetomaster[i][1], 1);
            execl("bin/polya_worker", "bin/polya_worker", NULL);
        }
        else{
            states[i]=0;
            problems[i]=NULL;
            close(readfrommaster[i][0]);
            close(writetomaster[i][1]);
        }
    }
    while(1){
        sigsuspend(&prev);
        int terminated=0;
        for(int i=0; i<workers; i++){
            if(terminated==workers){
                exit(EXIT_SUCCESS);
            }
            if(states[i]==1){//IDLE state
                struct problem *variant=get_problem_variant(workers, i);
                if(variant){
                    problems[i]=variant;
                    sf_send_problem(pids[i], variant);
                    debug("pid i is %d", pids[i]);
                    kill(pids[i],SIGCONT);
                    sf_change_state(pids[i], 1, 2);
                    states[i]=2;//change state to continue;
                }
                else{
                    kill(pids[i],SIGTERM);
                }
            }
            else if(states[i]==3){
                struct problem *variant=problems[i];
                if(variant){
                    debug("write to child ---------------------------------------------------");
                    write(readfrommaster[i][1], variant, variant->size);
                }
            }
            else if(states[i]==4){
                if(problems[i]==NULL){
                    states[i]=1;
                    continue;
                }
                struct result *results=malloc(sizeof(struct result));
                read(writetomaster[i][0], results, sizeof(struct result));
                size_t data=results->size-sizeof(struct result);
                results=realloc(results, results->size);
                read(writetomaster[i][0], (void *)results+sizeof(struct result), data);
                sf_recv_result(pids[i], results);
                if(!post_result(results, problems[i])){//if child have correct solution, post it and send sighup
                    for(int k=0; k<workers; k++){
                        if(problems[k]==problems[i]&&k!=i){
                            sf_cancel(pids[k]);
                            kill(pids[k], SIGHUP);
                            problems[k]=NULL;
                        }
                    }
                    problems[i]=NULL;
                }

                states[i]=1;
            }
            else if(states[i]==5||states[i]==6){
                terminated++;
            }
        }
    }
        // debug("find out one idle pid, check state, do corresponding work, current pid is: %d",current);
        // for(int j=0; j<workers; j++){
        //     debug("current pid is %d",pid[j]);
        //     if(pid[j]==current){
        //         debug("current state is %d", state[j]);
        //         if(state[j]==1||state[j]==4){
        //             debug("state change from start to idle");
        //             state[j]=2;//change state to idle;
        //             struct problem *variant=get_problem_variant(workers, j);
        //             if(variant){
        //                 problems[j]=variant;
        //                 sf_send_problem(pid[j], variant);
        //                 debug("pid j is %d", pid[j]);
        //                 kill(pid[j],SIGCONT);
        //                 sf_change_state(pid[j], 2, 3);
        //                 state[j]=3;//change state to continue;
        //             }
        //             else{
        //                 int idles=0;
        //                 for(int m=0; m<workers; m++){
        //                     if(state[m]==2){
        //                         idles++;
        //                     }
        //                 }
        //                 if(idles==workers){
        //                     for(int n=0; n<workers; n++){
        //                         kill(pid[n], SIGTERM);
        //                     }
        //                     int result;
        //                     while((result=waitpid(-1, NULL, WNOHANG))!=-1);
        //                     sf_end();
        //                     exit(EXIT_SUCCESS);
        //                 }
        //             }
        //             break;
        //         }
        //         else if(state[j]==3){
        //             struct problem *variant=problems[j];
        //             if(variant){
        //                 debug("write to child ---------------------------------------------------");
        //                 state[j]=4;//change state to runing
        //                 sf_change_state(pid[j], 3, 4);
        //                 write(readfrommaster[j][1], variant, variant->size);
        //             }
        //             else{
        //                 state[j]=2;
        //                 sf_change_state(pid[j], 3, 2);
        //             }
        //             break;
        //         }
        //         else if(state[j]==4){
        //             debug("read from child----------------------------------------------------");
        //             state[j]=2;//change state to stop/which is idle
        //             sf_change_state(pid[j], 4, 2);
        //             if(problems[j]==NULL){
        //                 break;
        //             }
        //             else{
        //                 struct result *results=malloc(sizeof(struct result));
        //                 read(writetomaster[j][0], results, sizeof(struct result));
        //                 size_t data=results->size-sizeof(struct result);
        //                 results=realloc(results, results->size);
        //                 read(writetomaster[j][0], (void *)results+sizeof(struct result), data);
        //                 sf_recv_result(pid[j], results);
        //                 if(!post_result(results, problems[j])){//if child have correct solution, post it and send sighup
        //                     for(int k=0; k<workers; k++){
        //                         if(problems[k]==problems[j]&&k!=j){
        //                             sf_cancel(pid[k]);
        //                             kill(pid[k], SIGHUP);
        //                             problems[k]=NULL;
        //                         }
        //                     }
        //                     problems[j]=NULL;
        //                 }
        //             }
        //             break;
        //         }
        //     }
    // }

    // TO BE IMPLEMENTED
    return EXIT_FAILURE;
}
