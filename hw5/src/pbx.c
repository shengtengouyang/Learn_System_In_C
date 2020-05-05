

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "debug.h"
#include "pbx.h"

struct pbx{
    TU *extensions[PBX_MAX_EXTENSIONS];
};

struct tu{
    TU_STATE state;
    TU *opponent;
    int fd;
};

static void write_message(int fd, TU_STATE state);

PBX *pbx_init(){
    PBX *init = malloc(sizeof(PBX));
    for(int i=0; i<PBX_MAX_EXTENSIONS; i++){
        init->extensions[i]=0;
    }
    return init;
}

void pbx_shutdown(PBX *pbx){

}

TU *pbx_register(PBX *pbx, int fd){
    debug("entering pbx_register with fd %d", fd);
    if(fd>PBX_MAX_EXTENSIONS){
        return NULL;
    }
    TU *newtu=malloc(sizeof(TU));
    newtu->state=TU_ON_HOOK;
    newtu->opponent=0;
    newtu->fd=fd;
    pbx->extensions[fd]=newtu;
    write_message(fd, TU_ON_HOOK);
    return newtu;
}


int pbx_unregister(PBX *pbx, TU *tu){
    int fd=tu->fd;
    free(tu);
    if(fd<4||fd>PBX_MAX_EXTENSIONS){
        return -1;
    }
    pbx->extensions[fd]=0;
    return 0;
}

int tu_fileno(TU *tu){
    int fd=tu->fd;
    if(fd<4||fd>PBX_MAX_EXTENSIONS){
        return -1;
    }
    return fd;
}

int tu_extension(TU *tu){
    return tu_fileno(tu);
}

int tu_pickup(TU *tu){
    debug("startpickup %p with state %d and opponent %p ", tu, tu->state, tu->opponent);
    TU *other=tu->opponent;
    switch(tu->state){
        case TU_ON_HOOK: tu->state=TU_DIAL_TONE; break;
        case TU_RINGING:
            tu->state=TU_CONNECTED;
            tu->opponent->state=TU_CONNECTED;
            break;
        default:
            break;
    }
    debug("current tu %d change state to %d", tu->fd, tu->state);
    write_message(tu->fd, tu->state);
    if(other){
        debug("other tu %d change state to %d", other->fd, other->state);
        write_message(other->fd, other->state);
    }
    return 0;
}

int tu_hangup(TU *tu){
    TU *other=tu->opponent;
    switch(tu->state){
        case TU_CONNECTED:
            tu->state=TU_ON_HOOK;
            other->state=TU_DIAL_TONE;
            break;
        case TU_RING_BACK:
            tu->state=TU_ON_HOOK;
            other->state=TU_ON_HOOK;
            break;
        case TU_RINGING:
            tu->state=TU_ON_HOOK;
            other->state=TU_DIAL_TONE;
            break;
        case TU_DIAL_TONE:
        case TU_BUSY_SIGNAL:
        case TU_ERROR:
            tu->state=TU_ON_HOOK;
            break;
        default:
            break;
    }
    write_message(tu->fd, tu->state);
    if(other){
        other->opponent=0;
        tu->opponent=0;
        write_message(other->fd, other->state);
    }
    return 0;
}

int tu_dial(TU *tu, int ext){
    int changed=0;
    if(pbx->extensions[ext]==0){
        tu->state=TU_ERROR;
    }
    else{
        if(tu->state==TU_DIAL_TONE){
            tu->opponent=pbx->extensions[ext];
            tu->opponent->opponent=tu;
            if(tu->opponent->state==TU_ON_HOOK){
                tu->state=TU_RING_BACK;
                tu->opponent->state=TU_RINGING;
                changed=1;
            }
            else{
                tu->state=TU_BUSY_SIGNAL;
                tu->opponent->opponent=0;
                tu->opponent=0;
            }
        }
    }
    write_message(tu->fd, tu->state);
    if(changed){
        write_message(tu->opponent->fd, tu->opponent->state);
    }
    return 0;
}

/*
 * "Chat" over a connection.
 *
 * If the state of the TU is not TU_CONNECTED, then nothing is sent and -1 is returned.
 * Otherwise, the specified message is sent via the network connection to the peer TU.
 * In all cases, the states of the TUs are left unchanged.
 *
 * @param tu  The tu sending the chat.
 * @param msg  The message to be sent.
 * @return 0  If the chat was successfully sent, -1 if there is no call in progress
 * or some other error occurs.
 */
int tu_chat(TU *tu, char *msg){
    if(tu->state!=TU_CONNECTED){
        return -1;
    }
    else{
        int len=1;
        char *ptr=msg;
        while(*ptr!='\0'){
            len++;
            ptr++;
        }
        write_message(tu->fd, tu->state);
        write(tu->opponent->fd, "CHAT ", 5);
        write(tu->opponent->fd, msg, len);
        write(tu->opponent->fd, EOL, 2);
    }
    return 0;
}

void write_message(int fd, TU_STATE state){
    debug("write message in state %d", state);
    char * msg=tu_state_names[state];
    char *ptr=msg;
    int len=1;
    while(*ptr!='\0'){
        len++;
        ptr++;
    }
    write(fd, msg, len);
    write(fd, EOL, 2);
}