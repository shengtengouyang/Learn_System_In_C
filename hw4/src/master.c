#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <unistd.h>
#include <sys/wait.h>
#define WORKER_INIT 0

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
            states[index]=WORKER_EXITED;
            sf_change_state(current,oldstate, WORKER_EXITED);
            worker_alive--;
        }
        else if(WIFCONTINUED(status)){
            states[index]=WORKER_RUNNING;
            sf_change_state(current, oldstate, states[index]);
            debug("worker (pid: %d, state: %d) has %s ", current, oldstate, "continued");
        }
        else if(WIFSTOPPED(status)){
            if(states[index]==WORKER_STARTED){
                states[index]=WORKER_IDLE;
            }
            else{
                states[index]=WORKER_STOPPED;
            }
            sf_change_state(current, oldstate, states[index]);
            debug("worker (pid: %d, state: %d) has %s ", current, oldstate, "stopped");
        }
        else{
            states[index]=WORKER_ABORTED;
            sf_change_state(current, oldstate, states[index]);
            worker_alive--;
        }
    }
    if(worker_alive==0){
        sf_end();
        exit(EXIT_SUCCESS);
    }
}

void idle_state(struct problem *problems[], int i, int workers, int writeside){
    debug("structuring problem for worker at index [ %d ]", i);
    struct problem *variant=get_problem_variant(workers, i);
    if(variant){
        problems[i]=variant;
        debug("save problem %d of %d into index [%d]----------------------------------------------------", problems[i]->var, problems[i]->id, i);
        debug("pid i is %d", pids[i]);
        kill(pids[i],SIGCONT);
        states[i]=WORKER_CONTINUED;//change state to continue;
        sf_change_state(pids[i], WORKER_IDLE, WORKER_CONTINUED);
        debug("write to child %d with problem :%d of %d---------------------------------------------------", i, problems[i]->var, problems[i]->id);
        sf_send_problem(pids[i], variant);
        write(writeside, variant, variant->size);
    }
    else{
        idle++;
        if(idle==workers){
            for(int i=0; i<workers; i++){
                debug("terminate the child %d with pid %d", i, pids[i]);
                kill(pids[i], SIGTERM);
                kill(pids[i], SIGCONT);
            }
        }
    }
}

void stopped_state(struct problem *problems[], int i, int workers, int readside){
    struct result *results=malloc(sizeof(struct result));
    read(readside, results, sizeof(struct result));
    size_t data=results->size-sizeof(struct result);
    results=realloc(results, results->size);
    read(readside, (void *)results+sizeof(struct result), data);
    sf_recv_result(pids[i], results);
    states[i]=WORKER_IDLE;
    sf_change_state(pids[i], WORKER_STOPPED, WORKER_IDLE);
    if(problems[i]&&!post_result(results, problems[i])){//if child have correct solution, post it and send sighup
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

            states[i]=WORKER_STARTED;
            sf_change_state(pids[i], WORKER_INIT, WORKER_STARTED);
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
        idle=0;
        for(int i=0; i<workers; i++){
            if(states[i]==WORKER_IDLE||states[i]==WORKER_STOPPED){//IDLE state or stopped state
                if(states[i]==WORKER_STOPPED){
                    stopped_state(problems, i, workers, writetomaster[i][0]);
                }
                idle_state(problems, i, workers, readfrommaster[i][1]);
            }
        }
    }

    // TO BE IMPLEMENTED
    return EXIT_FAILURE;
}
