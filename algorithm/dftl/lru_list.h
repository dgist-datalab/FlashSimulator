#ifndef __DFTL_LRU__
#define __DFTL_LRU__

#include <stdio.h>
#include <stdlib.h>

typedef struct __node{
	void *DATA;
	struct __node *next;
	struct __node *prev;
} NODE;

typedef struct __lru{
	NODE *head;
	NODE *tail;
} LRU;

void lru_init(LRU**);
void lru_free(LRU*);
NODE* lru_push(LRU*, void*);
void lru_delete(LRU*, NODE*);
void* lru_pop(LRU*);
void lru_update(LRU*, NODE*);
void lru_print(LRU*);
void lru_print_back(LRU*);

#endif
