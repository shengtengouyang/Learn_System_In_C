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
#define M 64
void *heap;
sf_block *prologue;
sf_block *epilogue;
int checkSize(size_t size);
void initLists();
void add_block();
sf_block *remove_block(sf_block *current);
sf_block *split(sf_block *current, size_t size, int isLast);
void coalesce(sf_block *current, sf_block *next, int isLast);

// sf_block *remove_first_block(int index){
//     sf_block *first=&sf_free_list_heads[index];
//     sf_block *second=first->body.links.next;
//     sf_block *third=second->body.links.next;
//     first->body.links.next=third;
//     third->body.links.prev=first;
//     second->body.links.prev=0;
//     second->body.links.next=0;
//     return second;
// }

sf_block *remove_block(sf_block *current){
    sf_block *next=current->body.links.next;
    sf_block *prev=current->body.links.prev;
    prev->body.links.next=next;
    next->body.links.prev=prev;
    current->body.links.prev=0;
    current->body.links.next=0;
    return current;
}

void add_block(sf_block *block, int index){
    sf_block *first=&sf_free_list_heads[index];
    sf_block *second=first->body.links.next;
    first->body.links.next=block;
    second->body.links.prev=block;
    block->body.links.next=second;
    block->body.links.prev=first;
}

sf_block *split(sf_block *current, size_t size, int isLast){
    size_t required_size=((size+8+63)/64)*64;
    size_t current_size=current->header&BLOCK_SIZE_MASK;
    remove_block(current);
    current->header|=THIS_BLOCK_ALLOCATED;
    sf_block *remain;
    sf_block *after;
    if(required_size<=current_size-64){
        remain=(void *)current+required_size;
        remain->header=(current_size-required_size)|PREV_BLOCK_ALLOCATED;
        current->header-=remain->header&BLOCK_SIZE_MASK;
        remain->prev_footer=0;
        after=(void *)remain+(remain->header&BLOCK_SIZE_MASK);
        after->prev_footer=remain->header;
        if(isLast){
            add_block(remain, 9);
        }
        else{
            int index=checkSize(size);
            add_block(remain, index);
        }
    }
    else{
        after=(void *)current+current_size;
        after->prev_footer=0;
    }
    return current;
}

void coalesce(sf_block *current, sf_block *next, int isLast){
    int prev_allocated=current->header&PREV_BLOCK_ALLOCATED;
    current->header=((void *)next-(void *)current+(next->header&BLOCK_SIZE_MASK))|prev_allocated;
    sf_block *after=(void *)current+(current->header&BLOCK_SIZE_MASK);
    after->prev_footer=current->header;
    remove_block(current);
    remove_block(next);
    if(isLast){
        add_block(current, 9);
    }
    else{
        int index=checkSize(current->header&BLOCK_SIZE_MASK);
        add_block(current, index);
    }
}

sf_block *new_page(sf_block *prev, sf_block *wild){
    void *end=sf_mem_end();
    epilogue=end-16;
    epilogue->header=THIS_BLOCK_ALLOCATED;
    wild->prev_footer=prev->header;
    wild->header=((void*)epilogue-(void*)wild);
    if(prev->header&THIS_BLOCK_ALLOCATED){
        wild->header|=PREV_BLOCK_ALLOCATED;
    }
    epilogue->prev_footer=wild->header;
    add_block(wild, 9);
    if(!(wild->header&PREV_BLOCK_ALLOCATED)){
        coalesce(prev, wild, 1);
        return prev;
    }
    return wild;
}
void *sf_malloc(size_t size) {
    if(heap==0){
        heap=sf_mem_grow();
        initLists();
        prologue=heap+48;
        prologue->header=M|THIS_BLOCK_ALLOCATED|PREV_BLOCK_ALLOCATED;
        new_page(prologue, (void*)prologue+64);
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
                size_t required_size=((size+8+63)/64)*64;
                int isLast=i==9?1:0;
                while(ptr!=&sf_free_list_heads[i]){
                    if(required_size<=(ptr->header&BLOCK_SIZE_MASK)){
                        return split(ptr, size, isLast);
                    }
                    else if(required_size>(ptr->header&BLOCK_SIZE_MASK)){
                        ptr=ptr->body.links.next;
                        continue;
                    }
                }
                if(isLast){
                    sf_block *newWild=sf_mem_grow()-16;
                    sf_block *prev=epilogue-(epilogue->prev_footer&BLOCK_SIZE_MASK);
                    new_page(prev, newWild);
                    return sf_malloc(size);
                }
            }
        }
    }
    return NULL;
}

void sf_free(void *pp) {
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    return NULL;
}

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

void initLists(){
    for(int i=0; i<NUM_FREE_LISTS; i++){
        sf_free_list_heads[i].body.links.prev=&sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.next=&sf_free_list_heads[i];
    }
}