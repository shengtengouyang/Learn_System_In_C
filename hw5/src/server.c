#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "pbx.h"
#include "server.h"


#define NUM_COMMANDS 4
static TU_COMMAND parse_message(char *msg);
static char *read_message(FILE *fp);


void *pbx_client_service(void *arg){
    debug("thread is started");
    FILE *fp;
    int connfdp=*(int *)arg;
    free(arg);
    pthread_detach(pthread_self());
    debug("thread is started");
    TU *client=pbx_register(pbx, connfdp);
    debug("register in client_service");
    fp=fdopen(connfdp, "r");
    while(1){
        debug("file opened for reading %p", fp);
        char * msgbuf=read_message(fp);
        if(msgbuf==NULL){
            debug("reading interrupted, ----------------------terminating");
            return 0;
        }
        debug("received msg: %s", msgbuf);
        TU_COMMAND cmd=parse_message(msgbuf);
        int ext;
        switch(cmd){
            case TU_PICKUP_CMD:tu_pickup(client); break;
            case TU_HANGUP_CMD:tu_hangup(client); break;
            case TU_DIAL_CMD:
                ext=atoi(msgbuf+5);
                tu_dial(client,ext);
                break;
            case TU_CHAT_CMD:
                msgbuf+=4;
                while(isspace(*msgbuf)){
                    msgbuf++;
                }
                tu_chat(client, msgbuf);
                break;
        }
    }
    return 0;
}

static char *read_message(FILE *fp){
    size_t len=1024;
    char * msgbuf=malloc(len);
    char *ptr=msgbuf;
    size_t countlen=0;
    while(1){
        if(countlen==len-1){
            if(len+1==0){
                *ptr='\0';
                break;
            }
            len=2*len;
            msgbuf=realloc(msgbuf, len);
        }
        *ptr=fgetc(fp);
        if(*ptr==EOF){
            return NULL;
        }
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
    return msgbuf;
}

static TU_COMMAND parse_message(char *msg){
    for(int i = 0; i < NUM_COMMANDS; i++) {
        if(strstr(msg, "chat")==msg){
            return TU_CHAT_CMD;
        }
        if(strstr(msg, "dial ")==msg){
            return TU_DIAL_CMD;
        }
        if(strcmp(msg, tu_command_names[i])==0&&i!=TU_DIAL_CMD&&i!=TU_CHAT_CMD){
            return i;
        }
    }
    return -1;
}