#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"
#include "csapp.h"

static void terminate(int status);
static void *thread(void *vargp);
static volatile sig_atomic_t finished;
void sighup_handler(int sig){
    finished=1;
    debug("receive sighup signal");
}
/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    int  listenfd, *connfdp;
    finished=0;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    //Signal(sighup, sighuphandler), without sa_restart
    struct sigaction action, old_action;
    action.sa_handler = sighup_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    if (sigaction(SIGHUP, &action, &old_action) < 0)
        unix_error("Signal error");
    //
    int cmd=0;
    if(argc==3){
        if(*argv[1]=='-'&&*(argv[1]+1)=='p'&&*(argv[1]+2)=='\0'){
            cmd=1;
        }
    }
    if(!cmd){
        debug("wrong command");
        exit(EXIT_FAILURE);
    }
    Signal(SIGPIPE, SIG_IGN);
    listenfd=Open_listenfd(argv[2]);
    // Perform required initialization of the PBX module.
    debug("Initializing PBX...");
    pbx = pbx_init();
    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfdp = malloc(sizeof(int));
        *connfdp =accept(listenfd,(SA *) &clientaddr, &clientlen);
        debug("receive a new connection request with connfdp: %d", *connfdp);
        if(finished){
            close(listenfd);
            terminate(EXIT_SUCCESS);
        }
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    fprintf(stderr, "You have to finish implementing main() "
	    "before the PBX server will function.\n");

    terminate(EXIT_FAILURE);
}

void *thread(void *vargp){
    debug("start a thread");
    pbx_client_service(vargp); /* Service client */
    return NULL;
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    debug("Shutting down PBX...");
    pbx_shutdown(pbx);
    debug("PBX server terminating");
    exit(status);
}
