

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <semaphore.h>
#include "debug.h"
#include "pbx.h"
#include "csapp.h"

struct pbx{
    TU *extensions[PBX_MAX_EXTENSIONS];
    sem_t mutex;
};

struct tu{
    TU_STATE state;
    TU *opponent;
    int fd;
    FILE *file;
    sem_t mutex;
};

static void write_message(TU *tu);
static void reorder_mutex(TU *tu, TU *other);
PBX *pbx_init(){
    PBX *init = malloc(sizeof(PBX));
    for(int i=0; i<PBX_MAX_EXTENSIONS; i++){
        init->extensions[i]=0;
    }
    sem_init(&init->mutex, 0, 1);
    return init;
}

void pbx_shutdown(PBX *pbx){
    P(&pbx->mutex);
    for(int i=0; i<PBX_MAX_EXTENSIONS; i++){
        TU * currentTU=pbx->extensions[i];
        if(currentTU!=0){
            debug("start shutting down tu: %d", currentTU->fd);
            shutdown(currentTU->fd, SHUT_RDWR);
            debug("start hangup tu: %d", currentTU->fd);
        }
    }
    V(&pbx->mutex);
}

TU *pbx_register(PBX *pbx, int fd){
    P(&pbx->mutex);
    debug("entering pbx_register with fd %d", fd);
    if(fd>PBX_MAX_EXTENSIONS){
        V(&pbx->mutex);
        return NULL;
    }
    TU *newtu=malloc(sizeof(TU));
    sem_init(&(newtu->mutex), 0, 1);
    P(&newtu->mutex);
    newtu->state=TU_ON_HOOK;
    newtu->opponent=0;
    newtu->fd=fd;
    newtu->file=fdopen(fd, "w");
    pbx->extensions[fd]=newtu;
    write_message(newtu);
    V(&newtu->mutex);
    V(&pbx->mutex);
    return newtu;
}


int pbx_unregister(PBX *pbx, TU *tu){
    P(&pbx->mutex);
    int fd=tu->fd;
    fclose(tu->file);
    free(tu);
    if(fd<4||fd>PBX_MAX_EXTENSIONS){
        V(&pbx->mutex);
        return -1;
    }
    pbx->extensions[fd]=0;
    V(&pbx->mutex);
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
    P(&tu->mutex);
    debug("startpickup %p with state %d and opponent %p ", tu, tu->state, tu->opponent);
    TU *other=tu->opponent;
    int changed=0;
    switch(tu->state){
        case TU_ON_HOOK: tu->state=TU_DIAL_TONE; break;
        case TU_RINGING:
            reorder_mutex(tu, other);
            changed=1;
            tu->state=TU_CONNECTED;
            other->state=TU_CONNECTED;
            break;
        default:
            break;
    }
    debug("current tu %d change state to %d", tu->fd, tu->state);
    write_message(tu);
    if(changed){
        debug("other tu %d change state to %d", other->fd, other->state);
        write_message(other);
        V(&other->mutex);
    }
    V(&tu->mutex);
    return 0;
}

int tu_hangup(TU *tu){
    debug("start executing hangup ");
    P(&tu->mutex);
    debug("real start executing hangup ");
    TU *other=tu->opponent;
    reorder_mutex(tu, other);
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
    debug("other address before: %p", other);
    write_message(tu);
    debug("other address after: %p", other);
    if(other){
        debug("opponent fd is : %d and state is %d", other->fd, other->state);
        other->opponent=0;
        tu->opponent=0;
        write_message(other);
        V(&other->mutex);
    }
    V(&tu->mutex);
    return 0;
}

int tu_dial(TU *tu, int ext){
    P(&pbx->mutex);
    P(&tu->mutex);
    int changed=0;
    if(tu->state==TU_DIAL_TONE){
        TU *other=pbx->extensions[ext];
        reorder_mutex(tu, other);
        if(other==0){
            tu->state=TU_ERROR;
            tu->opponent=0;
        }
        else if(other->state==TU_ON_HOOK){
            tu->state=TU_RING_BACK;
            tu->opponent=other;
            other->state=TU_RINGING;
            other->opponent=tu;
            changed=1;
        }
        else{
            if(tu!=other){
                V(&other->mutex);
            }
            tu->state=TU_BUSY_SIGNAL;
        }
    }
    write_message(tu);
    V(&tu->mutex);
    if(changed){
        write_message(tu->opponent);
        V(&tu->opponent->mutex);
    }
    V(&pbx->mutex);
    return 0;
}

int tu_chat(TU *tu, char *msg){
    P(&tu->mutex);
    write_message(tu);
    if(tu->state!=TU_CONNECTED){
        V(&tu->mutex);
        return -1;
    }
    else{
        reorder_mutex(tu, tu->opponent);
        // int len=1;
        // char *ptr=msg;
        // while(*ptr!='\0'){
        //     len++;
        //     ptr++;
        // }
        FILE *file=tu->opponent->file;
        fputs("CHAT ",file);
        fputs(msg,file);
        fputs(EOL,file);
        fflush(file);
        V(&tu->opponent->mutex);
        // fclose(file);
        // write(tu->opponent->fd, "CHAT ", 5);
        // write(tu->opponent->fd, msg, len);
        // write(tu->opponent->fd, EOL, 2);
    }
    V(&tu->mutex);
    return 0;
}

void write_message(TU *tu){
    int fd=tu->fd;
    TU_STATE state=tu->state;
    debug("write message in state %d to fd: %d", state, fd);
    char * msg=tu_state_names[state];
    FILE *file=tu->file;
    fputs(msg, file);
    // char *ptr=msg;
    // int len=1;
    // while(*ptr!='\0'){
    //     len++;
    //     ptr++;
    // }
    debug("write down message: %s",msg);
    // debug("got blocked writing");
    // write(fd, msg, len);
    if(state==TU_ON_HOOK){
        char num[4];
        sprintf(num, "%d", fd);
        fputs(" ", file);
        fputs(num, file);
        // write(fd, " ", 1);
        // write(fd, num, len);
    }
    else if(state==TU_CONNECTED){
        char num[4];
        sprintf(num, "%d", tu->opponent->fd);
        fputs(" ", file);
        fputs(num, file);
        // char num[4];
        // int len=sprintf(num, "%d", tu->opponent->fd);
        // write(fd, " ", 1);
        // write(fd, num, len);
    }
    fputs(EOL, file);
    fflush(file);
    // fclose(file);
    // write(fd, EOL, 2);
    debug("ends writing");
}

void reorder_mutex(TU *tu, TU *other){
    if(tu==other||other==0){
        return;
    }
    if(&tu->mutex>&other->mutex){
        P(&other->mutex);
    }
    else{
        V(&tu->mutex);
        P(&other->mutex);
        P(&tu->mutex);
    }
}