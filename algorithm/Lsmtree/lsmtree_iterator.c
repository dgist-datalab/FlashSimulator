#include "lsmtree.h"
#include "level_op_iterator.h"
#include "../../include/data_struct/list.h"
#include "lsmtree_transaction.h"
#include "transaction_table.h"
#include "../../include/utils/kvssd.h"
#include "../../include/sem_lock.h"
#include "../../interface/koo_inf.h"
#include <stdlib.h>

#define ITERREADVALUE 152

extern lsmtree LSM;
extern lmi LMI;
extern my_tm _tm;

//static int iter_num=0;
static pthread_mutex_t cnt_lock=PTHREAD_MUTEX_INITIALIZER;
bool iterator_debug=false;

typedef struct lsm_range_get_params{
	level_op_iterator **loi;
	uint32_t total_loi_num;
	uint32_t read_target_num;
	uint32_t read_num;
	bool read_done;
	list *algo_params_list;
}range_get_params;

typedef struct algo_lsm_range_params{
	uint32_t lev;
	uint32_t idx;
	uint32_t offset;
	KEYT key;
	char *copied_data;
	value_set *value;
}algo_lsm_range_params;

typedef struct algo_lsm_preread_struct{
	uint32_t read_target_num;
	ppa_t transaction_ppa;
	ppa_t *ppa_list;
}algo_lsm_preread_struct;

void *lsm_range_end_req(algo_req *const al_req);

void __iterator_issue_read(ppa_t ppa, uint32_t i, uint32_t j, request *req){
	algo_req *al_req=(algo_req*)malloc(sizeof(algo_req));
	range_get_params *params=(range_get_params*)req->params;

	algo_lsm_range_params *al_params=(algo_lsm_range_params*)malloc(sizeof(algo_lsm_range_params));
	al_req->ppa=ppa;
	al_req->parents=req;
	al_req->type=HEADERR;
	al_req->end_req=lsm_range_end_req;
	al_req->params=al_params;

	al_params->lev=i;
	al_params->idx=j;
	al_params->value=inf_get_valueset(NULL, FS_MALLOC_R,PAGESIZE);

	list_insert(params->algo_params_list, (void*)al_params);

	LSM.li->read(al_req->ppa, PAGESIZE, al_params->value, ASYNC, al_req);
}

void copy_key_value_to_buf(char *buf, KEYT key, char *data){
	uint32_t offset=0;
	*(uint16_t*)&buf[offset]=key.len;
	offset+=2;
	memcpy(&buf[offset], key.key, key.len);
	offset+=key.len;
	if(data){
		*(uint16_t*)&buf[offset]=ITERREADVALUE;
		offset+=2;
		if(offset>(2*M)){
			printf("buffer over flow!!!! %s:%d\n", __FILE__, __LINE__);
		}
		memcpy(&buf[offset], data, ITERREADVALUE);
		offset+=ITERREADVALUE;
	}
}

void print_buffer(char *buf, uint32_t len){
	uint32_t offset=0;
	uint16_t value_len=0;
	while(1){
		KEYT temp;
		temp.len=*(uint16_t*)&buf[offset];
		if(temp.len==1 || temp.len==0 || offset >= len) break;
		offset+=2;
		temp.key=&buf[offset];
		offset+=temp.len;
		value_len=*(uint16_t*)&buf[offset];
		offset+=2;
		offset+=value_len;
		print_key(temp, true);
	}
}

void *lsm_range_end_req(algo_req *const al_req){
	request *req=al_req->parents;
	range_get_params *rgparams=(range_get_params*)req->params;
	algo_lsm_range_params *al_params=(algo_lsm_range_params*)al_req->params;
	switch(al_req->type){
		case HEADERR:
			al_params->copied_data=(char*)malloc(PAGESIZE);
			memcpy(al_params->copied_data, al_params->value->value, PAGESIZE);
			inf_free_valueset(al_params->value ,FS_MALLOC_R);
			break;
		case DATAR:
	//		printf("%d iter key :%.*s\n", iter_num++,KEYFORMAT(al_params->key));
			if(al_params->value->ppa%NPCINPAGE){
				if(req->buf)
					copy_key_value_to_buf(&req->buf[al_params->offset], al_params->key, &al_params->value->value[4096]);	
			}
			else{
				if(req->buf)
					copy_key_value_to_buf(&req->buf[al_params->offset], al_params->key, al_params->value->value);	
			}
			inf_free_valueset(al_params->value, FS_MALLOC_R);
			free(al_params->key.key);
			free(al_params);
			break;
	}

	pthread_mutex_lock(&cnt_lock);
	rgparams->read_num++;
	if(rgparams->read_num == rgparams->read_target_num){
		pthread_mutex_unlock(&cnt_lock);
		if(al_req->type==HEADERR){
			if(rgparams->read_done){
				printf("error double retry %s:%d\n", __FILE__, __LINE__);
				abort();
			}
			//printf("retry!!!!\n");
			rgparams->read_done=true;
			inf_assign_try(req);
		}
		else{
			fdriver_unlock(&LSM.iterator_lock);
			req->end_req(req);
			for(int32_t i=rgparams->total_loi_num-1; i>=0; i--){
				level_op_iterator_free(rgparams->loi[i]);
			}
			free(rgparams->loi);
			free(rgparams);
		}
	}
	else{
		pthread_mutex_unlock(&cnt_lock);
	}
	free(al_req);
	return NULL;
}

extern char *debug_koo_key;

inline static uint32_t __lsm_range_KV(request *const req, range_get_params *rgparams, skiplist *temp_list){
	snode *t_node;
	uint32_t offset=0, pre_offset=0;
	rgparams->read_target_num=temp_list->size>req->length?req->length:temp_list->size;
	req->length=rgparams->read_target_num;
	uint32_t i=0;
	int32_t j;
	bool break_flag=false;
	algo_req *al_req;
	algo_lsm_range_params *al_params;
	for_each_sk(temp_list, t_node){
		if(i+1==req->length) {
			break_flag=true;
		}
		if(t_node->ppa==UINT32_MAX){
			//copy value
			if(req->buf)
				copy_key_value_to_buf(&req->buf[offset], t_node->key, t_node->value.g_value);
			//printf("target %d iter key :%.*s\n", iter_num++,KEYFORMAT(t_node->key));

			pthread_mutex_lock(&cnt_lock);
			rgparams->read_num++;
			if(rgparams->read_num==rgparams->read_target_num){
				pthread_mutex_unlock(&cnt_lock);
				fdriver_unlock(&LSM.iterator_lock);
	
				req->buf_len=offset;

				req->end_req(req);
				for(j=rgparams->total_loi_num-1; j>=0; j--){
					level_op_iterator_free(rgparams->loi[j]);
				}
				free(rgparams->loi);
				free(rgparams);
				break;
			}
			else{
				pthread_mutex_unlock(&cnt_lock);
			}
			goto next_round;
		}
		else if(t_node->ppa==TOMBSTONE){

			pthread_mutex_lock(&cnt_lock);
			req->length=rgparams->read_target_num=rgparams->read_target_num-1;
			if(rgparams->read_num==rgparams->read_target_num){
				pthread_mutex_unlock(&cnt_lock);
				fdriver_unlock(&LSM.iterator_lock);
	
				req->buf_len=pre_offset;

				req->end_req(req);
				for(j=rgparams->total_loi_num-1; j>=0; j--){
					level_op_iterator_free(rgparams->loi[j]);
				}
				free(rgparams->loi);
				free(rgparams);
				break;
			}
			else{
				pthread_mutex_unlock(&cnt_lock);
			}
			goto next_round;
		}
		al_req=(algo_req*)malloc(sizeof(algo_req));
		al_params=(algo_lsm_range_params*)malloc(sizeof(algo_lsm_range_params));
		al_req->ppa=t_node->ppa;
		al_req->parents=req;
		al_req->type=DATAR;
		al_req->end_req=lsm_range_end_req;
		al_req->params=al_params;

		al_params->value=inf_get_valueset(NULL, FS_MALLOC_R,PAGESIZE);
		kvssd_cpy_key(&al_params->key,&t_node->key);
		al_params->offset=offset;
		al_params->value->ppa=al_req->ppa;
		req->buf_len=offset+t_node->key.len+2+2+ITERREADVALUE;
		LSM.li->read(al_req->ppa/NPCINPAGE, PAGESIZE, al_params->value, ASYNC, al_req);	
next_round:
		pre_offset=offset;
		offset+=t_node->key.len+2+2+ITERREADVALUE;
		i++;
		if(break_flag) break;
	}
	return 1;
}

inline static uint32_t __lsm_range_key(request *const req, range_get_params *rgparams, skiplist *temp_list){
	snode *t_node;
	uint32_t offset=0;
	bool break_flag=false;
	rgparams->read_target_num=temp_list->size>req->length?req->length:temp_list->size;
	req->length=rgparams->read_target_num;
	uint32_t i=0;
	//uint32_t iter_num=0;
	for_each_sk(temp_list, t_node){
		if(i+1==req->length){
			break_flag=true;
		}
		//printf("%d iter key :%.*s\n", iter_num++,KEYFORMAT(t_node->key));
		if(req->buf)
			copy_key_value_to_buf(&req->buf[offset], t_node->key, NULL);
		offset+=t_node->key.len+1;
		i++;
		if(break_flag) break;
	}
	fdriver_unlock(&LSM.iterator_lock);

	req->end_req(req);
	for(int32_t i=rgparams->total_loi_num-1; i>=0; i--){
		level_op_iterator_free(rgparams->loi[i]);
	}
	free(rgparams->loi);
	free(rgparams);
	return 1;
}

uint32_t __lsm_range_get(request *const req){ //after range_get
	range_get_params *rgparams=(range_get_params*)req->params;
	li_node *temp;
	level_op_iterator **loi=rgparams->loi;
	for_each_list_node(rgparams->algo_params_list, temp){
		algo_lsm_range_params *al_params=(algo_lsm_range_params*)temp->data;
		level_op_iterator_set_iterator(loi[al_params->lev], al_params->idx, al_params->copied_data, req->key, req->offset);
		free(al_params);
	}
	list_free(rgparams->algo_params_list);


	rgparams->read_target_num=rgparams->read_num=0;

	skiplist *temp_list=skiplist_init();
	snode *tt;
	for(int32_t i=rgparams->total_loi_num-1; i>=0; i--){
		ka_pair t;
		if(!loi[i]) continue;
		while(level_op_iterator_pick_key_addr_pair(loi[i],&t)){
			tt=skiplist_insert_iter(temp_list, t.key, t.ppa);
			if(t.ppa==UINT32_MAX){
				tt->value.g_value=t.data;
			}
			level_op_iterator_move_next(loi[i]);
		}
	}

	if(req->length < temp_list->size){
		req->parents->eof=1;
	}
	else{
		req->parents->eof=0;
	}

	req->length=req->length > temp_list->size? temp_list->size :req->length;
	

	uint32_t res=0;
	if(!req->length){
		//printf("lsm_range_get: no data to read!\n");
		req->length=0;
		fdriver_unlock(&LSM.iterator_lock);
		for(int32_t i=rgparams->total_loi_num-1; i>=0; i--){
			level_op_iterator_free(rgparams->loi[i]);
		}
		free(rgparams->loi);
		free(rgparams);
		skiplist_free_iter(temp_list);
		req->end_req(req);
		return res;
	}
	if(req->type==FS_RANGEGET_T){
		res=__lsm_range_KV(req, rgparams, temp_list);
	}
	else if(req->type==FS_KEYRANGE_T){
		res=__lsm_range_key(req, rgparams, temp_list);
	}else{
		printf("unknown req->type %s:%d\n", __FILE__, __LINE__);
		abort();
	}

	skiplist_free_iter(temp_list);
	return res;
}


uint32_t lsm_range_get(request *const req){
	//req->magic is include flag
	compaction_wait_jobs();
	if(req->params){
		return __lsm_range_get(req);
	}

	req->length=req->length > (2*M)/(256+2+2+ITERREADVALUE) ? (2*M)/(256+2+2+ITERREADVALUE):req->length;

	fdriver_lock(&LSM.iterator_lock);

	range_get_params *params=(range_get_params*)malloc(sizeof(range_get_params));
	params->read_target_num=0;
	params->read_num=0;
	params->read_done=false;
	params->algo_params_list=list_init();
	req->params=params;
/*
	if(ISTRANSACTION(LSM.setup_values)){
		transaction_table_print(_tm.ttb, false);
	}
	LSM.lop->print_level_summary();
*/

	//printf("target iteration key --- %.*s\n", KEYFORMAT(req->key));
	uint32_t target_trans_entry_num=0;
	transaction_entry **trans_sets=NULL;
	if(ISTRANSACTION(LSM.setup_values)){
		//find target transaction;
		target_trans_entry_num=transaction_table_iterator_targets(_tm.ttb, req->key, req->tid, &trans_sets);
	}
	else{
		target_trans_entry_num=1; //for memtable in LSMTREE
	}

	params->loi=(level_op_iterator**)malloc(sizeof(level_op_iterator*)*(LSM.LEVELN+target_trans_entry_num));
	params->total_loi_num=LSM.LEVELN+target_trans_entry_num;


	algo_lsm_preread_struct *_rt=(algo_lsm_preread_struct*)calloc( (LSM.LEVELN + target_trans_entry_num), sizeof(algo_lsm_preread_struct));

	uint32_t ppa;
	bool should_read;
	bool nothing_flag=true;
	bool noread=true;
	if(ISTRANSACTION(LSM.setup_values)){
		for(uint32_t i=0; i<target_trans_entry_num; i++){
			params->loi[target_trans_entry_num-1-i]=level_op_iterator_transact_init(trans_sets[i], req->key, &ppa, req->offset, &should_read);
			if(params->loi[target_trans_entry_num-1-i]){
				nothing_flag=false;
			}

			if(should_read){
	//			__iterator_issue_read(ppa, target_trans_entry_num-1-i, 0, req);
				noread=false;
				params->read_target_num++;
			}

			_rt[target_trans_entry_num-1-i].read_target_num=should_read?1:0;
			_rt[target_trans_entry_num-1-i].transaction_ppa=ppa;
		}
		free(trans_sets);
	}else{
		params->loi[0]=level_op_iterator_skiplist_init(LSM.memtable, req->key, req->offset);

		if(params->loi[0]){
			nothing_flag=false;
		}
	}

	uint32_t *ppa_list=NULL, read_num=0;
	uint32_t meta_read_target=req->length;
	for(int32_t i=LSM.LEVELN-1; i>=0; i--){
		read_num=0;
		params->loi[i+target_trans_entry_num]=level_op_iterator_init(LSM.disk[i], req->key, &ppa_list, &read_num, meta_read_target, req->offset, &should_read);

		if(should_read){
			/*
			for(uint32_t j=0; ppa_list[j]!=UINT_MAX; j++){
				if(j==UINT_MAX-1) continue;
				__iterator_issue_read(ppa_list[j], i+target_trans_entry_num, j, req);
			}
			free(ppa_list);*/
			noread=false;
		}

		_rt[target_trans_entry_num+i].read_target_num=should_read?read_num:0;
		_rt[target_trans_entry_num+i].ppa_list=ppa_list;
		params->read_target_num+=read_num;

		if(params->loi[i+target_trans_entry_num]){
			nothing_flag=false;
		}
	}

	if(nothing_flag){
	//	printf("lsm_range_get: no data!\n");
		req->length=0;
		fdriver_unlock(&LSM.iterator_lock);
		for(int32_t i=params->total_loi_num-1; i>=0; i--){
			level_op_iterator_free(params->loi[i]);
		}
		free(params->loi);
		free(params);
		free(_rt);
		req->end_req(req);
	}
	else if(noread){
	//	printf("lsm_range_get: no read!\n");
		free(_rt);
		return __lsm_range_get(req);
	}
	else{
		for(uint32_t i=0; i<LSM.LEVELN + target_trans_entry_num; i++){
			algo_lsm_preread_struct *trt=&_rt[i];
			for(uint32_t j=0; j<trt->read_target_num; j++){
				if(trt->ppa_list){
					if(trt->ppa_list[j]==UINT_MAX-1) continue;
					LMI.iteration_map_read_cnt++;
					__iterator_issue_read(trt->ppa_list[j], i, j, req);
				}
				else{
					LMI.iteration_map_read_cnt++;
					__iterator_issue_read(trt->transaction_ppa, i, 0, req);
				}
			}
		}

		for(uint32_t i=0; i<LSM.LEVELN + target_trans_entry_num; i++){
			free(_rt->ppa_list);
		}
		free(_rt);
	//	printf("read issues!\n");
	}

	return 1;
}
