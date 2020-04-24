#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <unistd.h>
#include <sys/wait.h>
#define INIT 0
#define START 1
#define IDLE 2
#define CONTINUED 3
#define RUNING 4
#define STOPPED 5
#define EXITED 6
#define ABORTED 7

/*
 * master
 * (See polya.h for specification.)
 */
static volatile sig_atomic_t states[MAX_WORKERS];
static volatile pid_t pids[MAX_WORKERS];
static volatile sig_atomic_t worker_alive;
static volatile sig_atomic_t idle;
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
    int status;
    pid_t current;
    int index;
    while(worker_alive>0&&(current=waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED))>0){
        index=findIndex(current);
        int oldstate=states[index];
        if(index==-1){
            return;
        }
        else if(WIFEXITED(status)){
            states[index]=EXITED;
            sf_change_state(current,oldstate, EXITED);
            worker_alive--;
        }
        else if(WIFCONTINUED(status)||WIFSTOPPED(status)){
            states[index]++;
            sf_change_state(current, oldstate, states[index]);
            debug("worker (pid: %d, state: %d) has %s ", current, oldstate, WIFSTOPPED(status)?"stopped":"continued");
        }
        else{
            states[index]=ABORTED;
            sf_change_state(current, oldstate, states[index]);
            worker_alive--;
        }
    }
    if(worker_alive==0){
        sf_end();
        exit(EXIT_SUCCESS);
    }
}

void idle_state(struct problem *problems[], int i, int workers){
    struct problem *variant=get_problem_variant(workers, i);
    if(variant){
        problems[i]=variant;
        debug("pid i is %d", pids[i]);
        kill(pids[i],SIGCONT);
        states[i]=CONTINUED;//change state to continue;
        sf_change_state(pids[i], IDLE, CONTINUED);
    }
    else{
        debug("terminate the child %d with pid %d", i, pids[i]);
        kill(pids[i], SIGCONT);
        kill(pids[i],SIGTERM);
    }
}

void running_state(struct problem *problems[], int i, int writeside){
    struct problem *variant=problems[i];
    if(variant){
        debug("write to child ---------------------------------------------------");
        sf_send_problem(pids[i], variant);
        write(writeside, variant, variant->size);
    }
}

void stopped_state(struct problem *problems[], int i, int workers, int readside){
    if(problems[i]){
        struct result *results=malloc(sizeof(struct result));
        read(readside, results, sizeof(struct result));
        size_t data=results->size-sizeof(struct result);
        results=realloc(results, results->size);
        read(readside, (void *)results+sizeof(struct result), data);
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
        free(results);
    }
    states[i]=IDLE;
    sf_change_state(pids[i], STOPPED, IDLE);
}

int master(int workers) {
    sf_start();
    signal(SIGCHLD, sigchld_handler);
    int readfrommaster[workers][2];
    int writetomaster[workers][2];
    struct problem  *problems [workers];
    worker_alive=0;
    idle=0;
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
            debug("Start worker %d", i);

            states[i]=START;
            sf_change_state(pids[i], INIT, START);
            problems[i]=NULL;
            close(readfrommaster[i][0]);
            close(writetomaster[i][1]);
            worker_alive++;
            debug("Workers alive %d", worker_alive);
            debug("Started worker %d", i);
        }
    }
    while(1){
        debug("Worker alive: %d", worker_alive);
        sigsuspend(&prev);
        for(int i=0; i<workers; i++){
            if(states[i]==IDLE||states[i]==STOPPED){//IDLE state or stopped state
                if(states[i]==STOPPED){
                    stopped_state(problems, i, workers, writetomaster[i][0]);
                }
                idle_state(problems, i, workers);
                    // if(problems[i]){
                    //     struct result *results=malloc(sizeof(struct result));
                    //     read(writetomaster[i][0], results, sizeof(struct result));
                    //     size_t data=results->size-sizeof(struct result);
                    //     results=realloc(results, results->size);
                    //     read(writetomaster[i][0], (void *)results+sizeof(struct result), data);
                    //     sf_recv_result(pids[i], results);
                    //     if(!post_result(results, problems[i])){//if child have correct solution, post it and send sighup
                    //         for(int k=0; k<workers; k++){
                    //             if(problems[k]==problems[i]&&k!=i){
                    //                 sf_cancel(pids[k]);
                    //                 kill(pids[k], SIGHUP);
                    //                 problems[k]=NULL;
                    //             }
                    //         }
                    //         problems[i]=NULL;
                    //     }
                    //     states[i]=IDLE;
                    //     sf_change_state(pids[i], STOPPED, IDLE);
                    // }
                // struct problem *variant=get_problem_variant(workers, i);
                // sf_change_state(pids[i], IDLE, CONTINUED);
                // if(variant){
                //     problems[i]=variant;
                //     sf_send_problem(pids[i], variant);
                //     debug("pid i is %d", pids[i]);
                //     kill(pids[i],SIGCONT);
                //     states[i]=CONTINUED;//change state to continue;
                // }
                // else{
                //     debug("terminate the child %d with pid %d", i, pids[i]);
                //     kill(pids[i], SIGCONT);
                //     kill(pids[i],SIGTERM);
                // }
            }
            else if(states[i]==RUNING){
                running_state(problems, i, readfrommaster[i][1]);
                // struct problem *variant=problems[i];
                // if(variant){
                //     debug("write to child ---------------------------------------------------");
                //     write(readfrommaster[i][1], variant, variant->size);
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
