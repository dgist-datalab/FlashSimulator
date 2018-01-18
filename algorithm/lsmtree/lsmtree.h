#ifndef __LSM_HEADER__
#define __LSM_HEADER__
#include <pthread.h>
#include "run_array.h"
#include "skiplist.h"
#include "../../include/container.h"
#include "../../include/settings.h"
#include "../../include/lsm_settings.h"

#define OLDDATA 1
#define HEADERR 2
#define HEADERW 3
#define DATAR 4
#define DATAW 5


typedef struct keyset{
	KEYT lpa;
	KEYT ppa;
}keyset;

typedef struct htable{
	keyset sets[KEYNUM];
	uint8_t bitset[KEYNUM/8];
#ifdef BLOOM
	BF* filter;
#endif
}htable;

typedef struct lsm_params{
	const request *req;
	pthread_mutex_t lock;
	uint8_t lsm_type;
	V_PTR value;
}lsm_params;


typedef struct lsmtree{
	struct level *disk[LEVELN];
	PTR level_addr[LEVELN];
	struct skiplist *memtable;
	struct skiplist *temptable;
	lower_info* li;
}lsmtree;

uint32_t lsm_create(lower_info *, algorithm *);
void lsm_destroy(lower_info*, algorithm*);
uint32_t lsm_get(const request*);
uint32_t lsm_set(const request*);
uint32_t lsm_remove(const request*);
void* lsm_end_req(struct algo_req*);
bool lsm_kv_validcheck(uint8_t *, int idx);
void lsm_kv_validset(uint8_t *,int idx);
keyset* htable_find(htable*, KEYT target);
#endif
