/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>
#define M 64
void *heap;
sf_block *prologue;
sf_block *epilogue;
int checkSize(size_t size);
int checkValid(void *ptr);
void initLists();
sf_block *new_page(sf_block *wild);
void add_block(sf_block *block, int index);
sf_block *remove_block(sf_block *current);
sf_block *split(sf_block *current, size_t size, int isLast, int alignment);
void coalesce(sf_block *current, sf_block *next, int isLast);

/*
 *  remove the block from the freelist (must pass in blocks that in the freelist)
 *  set up the relationship between current block and the block after it.
 */
sf_block *remove_block(sf_block *current){
    current->header|=THIS_BLOCK_ALLOCATED;
    sf_block *after=(void *)current+(current->header&BLOCK_SIZE_MASK);
    after->header|=PREV_BLOCK_ALLOCATED;
    sf_block *next=current->body.links.next;
    sf_block *prev=current->body.links.prev;
    prev->body.links.next=next;
    next->body.links.prev=prev;
    current->body.links.prev=0;
    current->body.links.next=0;
    return current;
}

/*
 *  add the block to the freelist at index: index, setup the linkedlist
 *  set up the relationship between the added block and the block right after it
 *  coalesce nearby blocks if available
*/
void add_block(sf_block *block, int index){

    sf_block *first=&sf_free_list_heads[index];
    sf_block *second=first->body.links.next;
    first->body.links.next=block;
    second->body.links.prev=block;
    block->body.links.next=second;
    block->body.links.prev=first;

    sf_block *after=(void *)block+(block->header&BLOCK_SIZE_MASK);
    if(block->header&THIS_BLOCK_ALLOCATED){
        block->header^=THIS_BLOCK_ALLOCATED;
        after->header^=PREV_BLOCK_ALLOCATED;
    }
    after->prev_footer=block->header;
    int isLast=index==9?1:0;
    int afterisLast=after==sf_free_list_heads[9].body.links.next?1:0;//check if after is the free wildness block
    if(!(block->header&PREV_BLOCK_ALLOCATED)){
        sf_block *prev=(void *)block-(block->prev_footer&BLOCK_SIZE_MASK);
        if(!(after->header&THIS_BLOCK_ALLOCATED)){
            coalesce(prev,after, afterisLast);
        }
        else{
            coalesce(prev, block, isLast);
        }
    }
    else{
        if(!(after->header&THIS_BLOCK_ALLOCATED)){
            coalesce(block, after, afterisLast);
        }
    }
}

/*
 *  split the current list with a certain size and certain alignment
 *  current has to be allocated block
 *  add the remaining part to the certain freelist
 *  setup the relationship between previous and remain
 *  check if there is splinter, if so, do not spilit
 *
 */
sf_block *split(sf_block *current, size_t size, int isLast, int alignment){
    size_t required_size=((size+8+alignment-1)/alignment)*alignment;
    size_t current_size=current->header&BLOCK_SIZE_MASK;
    // int allocated=0;
    // if(current->header&THIS_BLOCK_ALLOCATED){
    //     allocated=1;
    // }
    // if(!allocated){
    //     remove_block(current);
    // }
    sf_block *remain;
    // sf_block *after;
    if(required_size<=current_size-M){
        remain=(void *)current+required_size;
        remain->header=(current_size-required_size)|PREV_BLOCK_ALLOCATED;
        current->header-=remain->header&BLOCK_SIZE_MASK;
        // if(!allocated){
        //     remain->prev_footer=0;
        // }
        // after=(void *)remain+(remain->header&BLOCK_SIZE_MASK);
        // after->prev_footer=remain->header;
        if(isLast){
            add_block(remain, 9);
        }
        else{
            int index=checkSize(size);
            add_block(remain, index);
        }
    }
    // else{
    //     if(!allocated){
    //         after=(void *)current+current_size;
    //         after->prev_footer=0;
    //     }
    // }
    return current;
}
/*
 *  coalesce two block, has to be free blocks
 *  first remove two block from corresponding freelist
 *  conbine them with a new headersize
 *  the new conbined would be temporary allocated until add_block for this block finished
 */
void coalesce(sf_block *current, sf_block *next, int isLast){
    int prev_allocated=current->header&PREV_BLOCK_ALLOCATED;
    sf_block *middle=(void *)current+(current->header&BLOCK_SIZE_MASK);
    current->header=((void *)next-(void *)current+(next->header&BLOCK_SIZE_MASK))|prev_allocated;
    remove_block(current);
    if(middle!=next){
        remove_block(middle);
    }
    remove_block(next);
    // sf_block *after=(void *)current+(current->header&BLOCK_SIZE_MASK);
    // after->prev_footer=current->header;
    if(isLast){
        add_block(current, 9);
    }
    else{
        int index=checkSize(current->header&BLOCK_SIZE_MASK);
        add_block(current, index);
    }
}
/*
 *  generage a new epilogue at the end of the heap
 *  if epilogue previously exist, new page start from the old epilogue,
 *  else, new page starts right after the prologue
 *  add the new page to the freelist
 *  set up the relationship between the new page and prev block
 */
sf_block *new_page(sf_block *wild){
    void *end=sf_mem_end();
    epilogue=end-16;
    epilogue->header=THIS_BLOCK_ALLOCATED;
    int prev_allocated=wild==((void*)prologue+M)?2:wild->header&PREV_BLOCK_ALLOCATED;
    wild->header=((void*)epilogue-(void*)wild)|prev_allocated;
    if(wild==(void*)prologue+M){
        wild->prev_footer=prologue->header;
    }
    // wild->prev_footer=prev->header;
    // wild->header=((void*)epilogue-(void*)wild);
    // if(prev->header&THIS_BLOCK_ALLOCATED){
    //     wild->header|=PREV_BLOCK_ALLOCATED;
    // }
    // epilogue->prev_footer=wild->header;
    add_block(wild, 9);
    return sf_free_list_heads[9].body.links.next;
    // if(!(wild->header&PREV_BLOCK_ALLOCATED)){
    //     coalesce(prev, wild, 1);
    //     return prev;
    // }
    // return wild;
}
void *sf_malloc(size_t size) {
    if(heap==0){
        heap=sf_mem_grow();
        initLists();
        prologue=heap+48;
        prologue->header=M|THIS_BLOCK_ALLOCATED|PREV_BLOCK_ALLOCATED;
        new_page((void*)prologue+M);
        return sf_malloc(size);
    }
    else{
        if(size == 0){
            return NULL;
        }
        else{
            int index=checkSize(size+8);
            for(int i=index; i<NUM_FREE_LISTS; i++){
                sf_block *ptr=sf_free_list_heads[i].body.links.next;
                size_t required_size=((size+8+M-1)/M)*M;
                int isLast=i==9?1:0;
                while(ptr!=&sf_free_list_heads[i]){
                    if(required_size<=(ptr->header&BLOCK_SIZE_MASK)){
                        ptr=remove_block(ptr);
                        ptr=split(ptr, size, isLast, M);
                        // if(!(ptr->header&THIS_BLOCK_ALLOCATED)){
                        // }
                        void *result=(void *)ptr+16;
                        return result;
                    }
                    else if(required_size>(ptr->header&BLOCK_SIZE_MASK)){
                        ptr=ptr->body.links.next;
                        continue;
                    }
                }
                if(isLast){
                    void *grow=sf_mem_grow();
                    if(!grow){
                        return NULL;
                    }
                    // sf_block *newWild=grow-16;
                    // sf_block *prev=(void *)epilogue-(epilogue->prev_footer&BLOCK_SIZE_MASK);
                    sf_block *newpage=new_page(epilogue);
                    if(newpage==0){
                        break;
                    }
                    return sf_malloc(size);
                }
            }
        }
    }
    return NULL;
}

void sf_free(void *pp) {
    if(!checkValid(pp)){
        abort();
    }
    pp=pp-16;
    sf_block *current=pp;
    // current->header^=THIS_BLOCK_ALLOCATED;
    sf_block *next=pp+(current->header&BLOCK_SIZE_MASK);
    int isLast;
    if(next==epilogue-(next->header&BLOCK_SIZE_MASK)){
        isLast=1;
    }
    else{
        isLast=0;
    }
    if(!isLast){
        int index=checkSize(current->header&BLOCK_SIZE_MASK);
        add_block(current, index);
    }
    else{
        add_block(current, 9);
    }
}

void *sf_realloc(void *pp, size_t rsize) {
    if(!checkValid(pp)){
        abort();
    }
    if(rsize==0){
        sf_free(pp);
        return NULL;
    }
    sf_block *ptr=pp-16;
    size_t required_size=(rsize+8+M-1)/M*M;
    if(required_size>(ptr->header&BLOCK_SIZE_MASK)){
        void *larger=sf_malloc(rsize);
        if(larger==0){
            return NULL;
        }
        memcpy(larger, pp, (ptr->header&BLOCK_SIZE_MASK)-8);
        sf_free(pp);
        return larger;
    }
    else{
        int isLast=0;
        if(pp-16+(ptr->header&BLOCK_SIZE_MASK)==epilogue){
            isLast=1;
        }
        return (void *)split(ptr, rsize, isLast, M)+16;
    }
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    if(align<M || align%2!=0){
        sf_errno=EINVAL;
        return NULL;
    }
    size_t attempt=size+align+M;
    void * initial =sf_malloc(attempt);
    sf_block *current=initial-16;
    int check=(uintptr_t)initial%align==0?1:0;
    int isLast=current==((void *)epilogue-(current->header&BLOCK_SIZE_MASK))?1:0;
    if(!check){
        size_t free_size=((uintptr_t)initial/align+1)*align-(uintptr_t)initial;
        // void *ptr=initial+x;
        // sf_block *next=ptr-16;
        current=split(current, free_size-8, isLast, M);
        current=initial+free_size-16;
        isLast=current==((void *)epilogue-(current->header&BLOCK_SIZE_MASK))?1:0;
        remove_block(current);
        sf_free(initial);
    }
    current=split(current, size, isLast, align);
    // if(!(current->header&THIS_BLOCK_ALLOCATED)){
    //     current=remove_block(current);
    // }
    return (void *)current+16;
}

/*
 * check if the ptr is valid for allocated aligned block's payload
 */
int checkValid(void *ptr){
    if(!ptr){
        return 0;
    }
    if((uintptr_t)ptr%M!=0){
        return 0;
    }
    ptr=ptr-16;
    sf_block *convert=(sf_block *) ptr;
    if(!(convert->header&THIS_BLOCK_ALLOCATED)){
        return 0;
    }
    size_t size=convert->header&BLOCK_SIZE_MASK;
    void *convert_header=&convert->header;
    void *convert_footer=&((sf_block *)(ptr+size))->prev_footer;
    if(convert_header<((void *)prologue+M+8)||convert_footer>(void *)epilogue){
        return 0;
    }
    if(!(convert->header&PREV_BLOCK_ALLOCATED)){
        size_t prev_size=convert->prev_footer&BLOCK_SIZE_MASK;
        sf_block *prev_block=ptr-prev_size;
        if(prev_block->header&THIS_BLOCK_ALLOCATED){
            return 0;
        }
    }
    return 1;
}

/*
 * find the index in the freelist to store a requested size
 */
int checkSize(size_t size){
    if(size<=M){
        return 0;
    }
    else if (size<=2*M){
        return 1;
    }
    else if(size<=3*M){
        return 2;
    }
    else if(size<=5*M){
        return 3;
    }
    else if(size<=8*M){
        return 4;
    }
    else if(size<=13*M){
        return 5;
    }
    else if(size<=21*M){
        return 6;
    }
    else if(size<=34*M){
        return 7;
    }
    else{
        return 8;
    }
}

/*
 * initial all node in freelist a sentinel
 */
void initLists(){
    for(int i=0; i<NUM_FREE_LISTS; i++){
        sf_free_list_heads[i].body.links.prev=&sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.next=&sf_free_list_heads[i];
    }
}