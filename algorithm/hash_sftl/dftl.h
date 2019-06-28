#ifndef __H_DFTL__
#define __H_DFTL__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include "../../interface/interface.h"
#include "../../interface/queue.h"
#include "../../include/container.h"
#include "../../include/dftl_settings.h"
#include "../../include/dl_sync.h"
#include "../../include/types.h"
#include "../../include/data_struct/demand_hash.h"
#include "../../include/data_struct/redblack.h"
#ifdef W_BUFF
#include "../Lsmtree/skiplist.h"
#endif
#include "../blockmanager/BM.h"
#include "lru_list.h"
#define TYPE uint8_t
#define DATA_R DATAR
#define DATA_W DATAW
#define MAPPING_R MAPPINGR  // async
#define MAPPING_W MAPPINGW
#define MAPPING_M ( MAPPINGR | 0x80 )  // polling
#define TGC_R GCMR
#define TGC_W GCMW
#define TGC_M ( GCMR | 0x80 )
#define DGC_R GCDR
#define DGC_W GCDW

#define EPP (PAGESIZE / 4)  // Number of table entries per page
#define D_IDX (lpa / EPP)   // Idx of directory table
#define P_IDX (lpa % EPP)   // Idx of page table


#define CLEAN 0
#define DIRTY 1

#if S_FTL
/* This variables redefined on deamdn_hash.h */
//#define BITMAP_SIZE (EPP / 8) // Bitmap_size for SFTL
//#define NODE_SIZE 12          // hash node size(Key + PPN + chain pointer)
//#define P_SIZE 4	      // Default bucket pointer size

#endif


// Page table data structure
typedef struct demand_mapping_table{
    int32_t ppa; //Index = lpa
} D_TABLE;

// Cache mapping table data strcuture
typedef struct cached_table{
    int32_t t_ppa;
    int32_t idx;
    D_TABLE *p_table;
    NODE *queue_ptr; // for dirty pages (or general use)
#if C_CACHE
    NODE *clean_ptr; // for clean pages
#endif
    bool state; // CLEAN or DIRTY
    bool flying;
    request **flying_arr;
    int32_t num_waiting;

#if S_FTL
	int32_t b_form_size;
	int32_t flying_mapping_size;
	int32_t bit_cnt;
	bool form_check;   //In-Flash = 0, Bitmap = 1
	bool evic_flag;
	bool gc_flag;
	bool *s_bitmap;
	bool *d_bitmap;
	hash_t *ht_ptr;

#endif
	uint32_t read_hit;
	uint32_t write_hit;

} C_TABLE;

// OOB data structure
typedef struct demand_OOB{
    int32_t lpa;
} D_OOB;

// SRAM data structure (used to hold pages temporarily when GC)
typedef struct demand_SRAM{
    int32_t origin_ppa;
    D_OOB OOB_RAM;
    int32_t *DATA_RAM;
} D_SRAM;

typedef struct demand_params{
    value_set *value;
    dl_sync dftl_mutex;
    TYPE type;
    KEYT seq;
} demand_params;

typedef struct read_params{
    int32_t t_ppa;
    uint8_t read;
    int32_t m_w_cnt;
    int32_t m_w_max;
} read_params;

typedef struct mem_table{
    D_TABLE *mem_p;
} mem_table;

struct prefetch_struct {
    KEYT ppa;
    snode *sn;
};

/* extern variables */
extern algorithm __demand;

extern LRU *lru;

extern b_queue *free_b;
extern Heap *data_b;
extern Heap *trans_b;

extern C_TABLE *CMT; // Cached Mapping Table
extern uint8_t *VBM;
extern mem_table *mem_arr;
extern b_queue *mem_q;
extern D_OOB *demand_OOB; // Page level OOB

extern BM_T *bm;
extern Block *t_reserved;
extern Block *d_reserved;

volatile extern int32_t trans_gc_poll;
volatile extern int32_t data_gc_poll;

extern int32_t num_page;
extern int32_t num_block;
extern int32_t p_p_b;
extern int32_t num_tpage;
extern int32_t num_tblock;
extern int32_t num_dpage;
extern int32_t num_dblock;
extern int32_t max_cache_entry;
extern int32_t num_max_cache;
extern int32_t max_clean_cache;
extern int32_t max_dirty_cache;

extern int32_t num_caching;

extern int32_t tgc_count;
extern int32_t dgc_count;
extern int32_t read_tgc_count;
extern int32_t tgc_w_dgc_count;

extern int32_t total_cache_size;
extern int32_t free_cache_size;
extern int32_t cache_mapping_size;
extern int32_t flying_mapping_size;
extern int32_t check_size;
extern bool global_gc_flag;

/* extern variables */

// dftl.c
uint32_t demand_create(lower_info*, algorithm*);
void     demand_destroy(lower_info*, algorithm*);
uint32_t demand_get(request *const);
uint32_t demand_set(request *const);
uint32_t demand_remove(request *const);
uint32_t demand_eviction(request *const, char, bool *, bool *);
uint32_t demand_hit_eviction(request *const, char, bool *, bool *,int32_t);
void    *demand_end_req(algo_req*);

// dftl_utils.c
algo_req* assign_pseudo_req(TYPE type, value_set *temp_v, request *req);
D_TABLE* mem_deq(b_queue *q);
void mem_enq(b_queue *q, D_TABLE *input);
void merge_w_origin(D_TABLE *src, D_TABLE *dst);
int lpa_compare(const void *a, const void *b);
int32_t tp_alloc(char, bool*);
int32_t dp_alloc();
value_set* SRAM_load(D_SRAM* d_sram, int32_t ppa, int idx, char t);
void SRAM_unload(D_SRAM* d_sram, int32_t ppa, int idx, char t);
void cache_show(char* dest);

// garbage_collection.c
int32_t tpage_GC();
int32_t dpage_GC();


//Ver : Hash
#if S_FTL
int32_t find_head_idx(uint32_t lpa);
int32_t set_entry(uint32_t lpa, int32_t ppa);
int32_t get_entry(hash_node *f_node, int32_t offset);

int32_t reset_hash_entry(uint32_t lpa);
void reset_bitmap(int32_t t_index);
void remove_entry(hash_t *ht_ptr);


void set_hash_entry(hash_t *ht_ptr, uint32_t lpa, int32_t ppa);
int32_t free_hash_entry(hash_t *ht_ptr, uint32_t lpa);

int32_t cache_mapped_size();
void hash_table_status();
int32_t cached_entries();
int32_t cached_buckets();
#endif


#endif