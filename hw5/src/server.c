#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "debug.h"
#include "pbx.h"
#include "server.h"

int parse_message(char *msg);

void *pbx_client_service(void *arg){
    int len=1024;
    FILE *fp;
    int connfdp=*(int *)arg;
    free(arg);
    pthread_detach(pthread_self());
    pbx_register(pbx, connfdp);
    fp=fdopen(connfdp, "r");
    while(1){
        len=1024;
        char * msgbuf=malloc(len);
        char *ptr=msgbuf;
        int countlen=0;
        while(1){
            if(countlen==len-1){
                len=2*len;
                msgbuf=realloc(msgbuf, len);
            }
            *ptr=fgetc(fp);
            countlen++;
            if(*ptr=='\r'){
                char temp=fgetc(fp);
                if(temp=='\n'){
                    *ptr='\0';
                    break;
                }
                else{
                    ptr++;
                    *ptr=temp;
                }
                countlen++;
            }
            ptr++;
        }
        debug("received msg: %s", msgbuf);
    }
    return 0;
}

int parse_message(char *msg){
    return 1;
}