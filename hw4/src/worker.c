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
    debug("sighup");
    canceledp=1;
}
void sigterm_handler(int sig){
    debug("sigterm");
    exit(EXIT_SUCCESS);
}

int worker(void) {
    debug("Start");
    signal(SIGHUP, sighup_handler);
    signal(SIGTERM, sigterm_handler);
    raise(SIGSTOP);
    debug("continue");
    while(1){
        struct problem * problems=malloc(sizeof(struct problem));
        ssize_t header=read(0, problems, sizeof(struct problem));
        size_t datasize=problems->size-sizeof(struct problem);
        problems=realloc(problems, problems->size);
        ssize_t data=read(0, (void *)problems+sizeof(struct problem), datasize);
        debug("read data: header: %ld, data: %ld", header, data);
        struct result * out=solvers[problems->type].solve(problems, &canceledp);
        if(!out){
            out=malloc(sizeof(struct result));
            out->size=sizeof(struct result);
            out->id=problems->id;
            out->failed=1;
        }
        write(1, out, out->size);
        free(problems);
        raise(SIGSTOP);
    }
    // TO BE IMPLEMENTED
    return EXIT_FAILURE;
}
