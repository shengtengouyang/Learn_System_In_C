#include <pthread.h>
#include <stdlib.h>
#include "pbx.h"
#include "server.h"

void *pbx_client_service(void *arg){
    int connefdp=*(int *)arg;
    free(arg);
    pthread_detach(pthread_self());
    pbx_register(pbx, connefdp);
    while(1){

    }
    return 0;
}