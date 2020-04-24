#include <stdlib.h>

#include "debug.h"
#include "polya.h"
#include <unistd.h>
/*
 * worker
 * (See polya.h for specification.)
 */

static volatile sig_atomic_t canceledp=0;
void sighup_handler(int sig){
    debug("sighup received --canceling current solution attempt");
    canceledp=1;
}
void sigterm_handler(int sig){
    debug("sigterm");
    exit(EXIT_SUCCESS);
}

int worker(void) {
    debug("Starting");
    signal(SIGHUP, sighup_handler);
    signal(SIGTERM, sigterm_handler);
    debug("continue");
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    while(1){
        debug("Idling");
        raise(SIGSTOP);
        canceledp=0;
        sigprocmask(SIG_SETMASK, &prev, NULL);
        struct problem * problems=malloc(sizeof(struct problem));
        debug("Reading problems");
        read(0, problems, sizeof(struct problem));
        size_t datasize=problems->size-sizeof(struct problem);
        problems=realloc(problems, problems->size);
        read(0, (void *)problems+sizeof(struct problem), datasize);
        debug("got problem size: %ld, type: %d, variant: %d ", problems->size, problems->type, problems->var);
        struct result * out=solvers[problems->type].solve(problems, &canceledp);
        sigprocmask(SIG_BLOCK, &mask, &prev);
        if(!out){
            debug("solved canceled");
            out=malloc(sizeof(struct result));
            out->size=sizeof(struct result);
            out->id=problems->id;
            out->failed=1;
            write(1, out, out->size);
            free(out);
        }
        else{
            debug("write result to master");
            write(1, out, out->size);
        }
        free(problems);
    }
    // TO BE IMPLEMENTED
    return EXIT_FAILURE;
}
