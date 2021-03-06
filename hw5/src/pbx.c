

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <semaphore.h>
#include "debug.h"
#include "pbx.h"
#include "csapp.h"

struct pbx{
    TU *extensions[PBX_MAX_EXTENSIONS+4];
    sem_t mutex;
    sem_t swap;
};

struct tu{
    TU_STATE state;
    TU *opponent;
    int fd;
    FILE *file;
    sem_t mutex;
};

static int write_message(TU *tu);
static void reorder_mutex(TU *tu, TU *other);
PBX *pbx_init(){
    PBX *init = malloc(sizeof(PBX));
    for(int i=0; i<PBX_MAX_EXTENSIONS+4; i++){
        init->extensions[i]=0;
    }
    sem_init(&init->mutex, 0, 1);
    sem_init(&init->swap, 0, 1);
    return init;
}

void pbx_shutdown(PBX *pbx){
    P(&pbx->mutex);
    for(int i=0; i<PBX_MAX_EXTENSIONS+4; i++){
        TU * currentTU=pbx->extensions[i];
        if(currentTU!=0){
            debug("start shutting down tu: %d", currentTU->fd);
            shutdown(currentTU->fd, SHUT_RDWR);
        }
    }
    V(&pbx->mutex);
    while(1){
        int tus=0;
        for(int i=0; i<PBX_MAX_EXTENSIONS+4; i++){
            if(pbx->extensions[i]==0){
                tus++;
            }
            else{
                debug("non empty tu is %d", pbx->extensions[i]->fd);
            }
        }
        debug("tu numers is %d, desired is %d",tus, PBX_MAX_EXTENSIONS+4);
        if(tus==PBX_MAX_EXTENSIONS+4){
            break;
        }
    }
    free(pbx);
}

TU *pbx_register(PBX *pbx, int fd){
    P(&pbx->mutex);
    debug("entering pbx_register with fd %d", fd);
    if(fd>PBX_MAX_EXTENSIONS+4||pbx->extensions[fd]!=NULL||fd<4){
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
    debug("leaving pbx_register with fd %d", fd);
    V(&pbx->mutex);
    return newtu;
}


int pbx_unregister(PBX *pbx, TU *tu){
    P(&pbx->mutex);
    debug("start unregister tu %d", tu->fd);
    int fd=tu->fd;
    if(fd<4||fd>PBX_MAX_EXTENSIONS+4||pbx->extensions[fd]!=tu){
        debug("error in unregister!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        V(&pbx->mutex);
        return -1;
    }
    fclose(tu->file);
    free(tu);
    pbx->extensions[fd]=0;
    debug("end unregister tu %d", tu->fd);
    V(&pbx->mutex);
    return 0;
}

int tu_fileno(TU *tu){
    int fd=tu->fd;
    if(fd<4||fd>PBX_MAX_EXTENSIONS+4||pbx->extensions[fd]!=tu){
        return -1;
    }
    return fd;
}

int tu_extension(TU *tu){
    return tu_fileno(tu);
}

int tu_pickup(TU *tu){
    P(&pbx->swap);
    P(&tu->mutex);
    int error=0;
    debug("startpickup %p with state %d and opponent %p ", tu, tu->state, tu->opponent);
    TU *other=tu->opponent;
    int changed=0;
    switch(tu->state){
        case TU_ON_HOOK: V(&pbx->swap);tu->state=TU_DIAL_TONE; break;
        case TU_RINGING:
            reorder_mutex(tu, other);
            changed=1;
            tu->state=TU_CONNECTED;
            other->state=TU_CONNECTED;
            break;
        default:
            V(&pbx->swap);
            break;
    }
    debug("current tu %d change state to %d", tu->fd, tu->state);
    error+=write_message(tu);
    if(changed){
        debug("other tu %d change state to %d", other->fd, other->state);
        error+=write_message(other);
        V(&other->mutex);
    }
    debug("end pickup %p with state %d and opponent %p ", tu, tu->state, tu->opponent);
    V(&tu->mutex);
    if(error){
        return -1;
    }
    return 0;
}

int tu_hangup(TU *tu){
    debug("start executing hangup ");
    P(&pbx->swap);
    P(&tu->mutex);
    int error=0;
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
    error+=write_message(tu);
    debug("other address after: %p", other);
    if(other){
        debug("opponent fd is : %d and state is %d", other->fd, other->state);
        other->opponent=0;
        tu->opponent=0;
        error+=write_message(other);
        V(&other->mutex);
    }
    debug("end executing hangup");
    V(&tu->mutex);
    if(error){
        return -1;
    }
    return 0;
}

int tu_dial(TU *tu, int ext){
    P(&pbx->mutex);
    P(&pbx->swap);
    P(&tu->mutex);
    debug("start dile from %d to %d", tu->fd, ext);
    int error=0;
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
    else{
        V(&pbx->swap);
    }
    error+=write_message(tu);
    if(changed){
        error+=write_message(tu->opponent);
        V(&tu->opponent->mutex);
    }
    debug("end dialing for tu: %d", tu->fd);
    V(&tu->mutex);
    V(&pbx->mutex);
    if(error){
        return -1;
    }
    return 0;
}

int tu_chat(TU *tu, char *msg){
    P(&pbx->swap);
    P(&tu->mutex);
    debug("start chatting for tu %d", tu->fd);
    int error=0;
    error+=write_message(tu);
    if(tu->state!=TU_CONNECTED){
        V(&tu->mutex);
        V(&pbx->swap);
        return -1;
    }
    else{
        reorder_mutex(tu, tu->opponent);
        FILE *file=tu->opponent->file;
        if(fputs("CHAT ",file)<0||fputs(msg,file)<0||fputs(EOL,file)<0){
            error++;
        }
        if(fflush(file)<0){
            error++;
        }
        V(&tu->opponent->mutex);
    }
    debug("end chatting for tu %d", tu->fd);
    V(&tu->mutex);
    if(error){
        return -1;
    }
    return 0;
}

int write_message(TU *tu){
    int fd=tu->fd;
    int error=0;
    TU_STATE state=tu->state;
    debug("write message in state %d to fd: %d", state, fd);
    char * msg=tu_state_names[state];
    FILE *file=tu->file;
    if(fputs(msg, file)<0){
        error++;
    }
    debug("write down message: %s",msg);
    // debug("got blocked writing");
    // write(fd, msg, len);
    if(state==TU_ON_HOOK||state==TU_CONNECTED){
        if(state==TU_CONNECTED){
            fd=tu->opponent->fd;
        }
        char num[4];
        if(sprintf(num, "%d", fd)<0||fputs(" ", file)<0||fputs(num, file)<0){
            error++;
        }
    }
    if(fputs(EOL, file)<0){
        error++;
    }
    if(fflush(file)<0){
        error++;
    }
    debug("ends writing");
    if(error){
        return 1;
    }
    return 0;
}
//caller of reorder ( P(&tu->mutex);)
//if A: A>B,   P(A), x P(B)
//if  B:        P(B), x P(A), B<A, reorder, V(B),    P(A), P(B)
void reorder_mutex(TU *tu, TU *other){
    if(tu==other||other==0){
        V(&pbx->swap);
        return;
    }
    if(&tu->mutex>&other->mutex){
        P(&other->mutex);
    }
    else{
        V(&tu->mutex);
        //if other finished, may affter self tu, then other is no longer other,
        //if other hasn't finished or hasen't changed anything for self state, other is still other, then go ahead lock
        P(&other->mutex);
        P(&tu->mutex);
    }
    V(&pbx->swap);
}