
#define _LARGEFILE64_SOURCE
#include "posix.h"
#include "../../include/settings.h"
#include "../../bench/bench.h"
#include "../../bench/measurement.h"
#include "../../interface/queue.h"
#include "../../algorithm/lsmtree/lsmtree.h"
#ifdef dftl
#include "../../algorithm/dftl/dftl.h"
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
//#include <readline/readline.h>
//#include <readline/history.h>

pthread_mutex_t fd_lock;
mem_seg *seg_table;
#if (ASYNC==1)
queue *p_q;
pthread_t t_id;
bool stopflag;
#endif

lower_info my_posix={
	.create=posix_create,
	.destroy=posix_destroy,
#if (ASYNC==1)
	.push_data=posix_make_push,
	.pull_data=posix_make_pull,
#elif (ASYNC==0)
	.push_data=posix_push_data,
	.pull_data=posix_pull_data,
#endif
	.device_badblock_checker=NULL,
#if (ASYNC==1)
	.trim_block=posix_make_trim,
#elif (ASYNC==0)
	.trim_block=posix_trim_block,
#endif
	.refresh=posix_refresh,
	.stop=posix_stop,
	.lower_alloc=NULL,
	.lower_free=NULL
};

#if (ASYNC==1)
void *l_main(void *__input){
	posix_request *inf_req;

	while(1){
		if(stopflag){
			//printf("posix bye bye!\n");
			pthread_exit(NULL);
			break;
		}
		if(!(inf_req=(posix_request*)q_dequeue(p_q))){
			continue;
		}
		switch(inf_req->type){
			case FS_LOWER_W:
				posix_push_data(inf_req->key, inf_req->size, inf_req->value, inf_req->isAsync, inf_req->upper_req);
				break;
			case FS_LOWER_R:
				posix_pull_data(inf_req->key, inf_req->size, inf_req->value, inf_req->isAsync, inf_req->upper_req);
				break;
			case FS_LOWER_T:
				posix_trim_block(inf_req->key, inf_req->isAsync);
				break;
		}
		free(inf_req);
	}
	return NULL;
}

void *posix_make_push(KEYT PPA, uint32_t size, value_set* value, bool async, algo_req *const req){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	p_req->type=FS_LOWER_W;
	p_req->key=PPA;
	p_req->value=value;
	p_req->upper_req=req;
	p_req->isAsync=async;
	p_req->size=size;
	
	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			flag=true;
		}
	}
	return NULL;
}

void *posix_make_pull(KEYT PPA, uint32_t size, value_set* value, bool async, algo_req *const req){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	p_req->type=FS_LOWER_R;
	p_req->key=PPA;
	p_req->value=value;
	p_req->upper_req=req;
	p_req->isAsync=async;
	p_req->size=size;
	
	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			flag=true;
		}
	}
	return NULL;
}

void *posix_make_trim(KEYT PPA, bool async){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	p_req->type=FS_LOWER_T;
	p_req->key=PPA;
	p_req->isAsync=async;

	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			flag=true;
		}
	}
	return NULL;
}
#endif

uint32_t posix_create(lower_info *li){
	li->NOB=_NOS;
	li->NOP=_NOP;
	li->SOB=BLOCKSIZE*BPS;
	li->SOP=PAGESIZE;
	li->SOK=sizeof(KEYT);
	li->PPB=_PPB;
	li->PPS=_PPS;
	li->TS=TOTALSIZE;

	printf("!!! posix memory ASYNC: %d!!!\n", ASYNC);
	li->write_op=li->read_op=li->trim_op=0;
	seg_table = (mem_seg*)malloc(sizeof(mem_seg)*li->NOB);
	for(uint32_t i = 0; i < li->NOB; i++){
		seg_table[i].storage = NULL;
		seg_table[i].alloc = 0;
	}
	pthread_mutex_init(&fd_lock,NULL);
	pthread_mutex_init(&my_posix.lower_lock,NULL);
	measure_init(&li->writeTime);
	measure_init(&li->readTime);
#if (ASYNC==1)
	stopflag = false;
	q_init(&p_q, 1024);
	pthread_create(&t_id,NULL,&l_main,NULL);
#endif
	return 1;
}

void *posix_refresh(lower_info *li){
	measure_init(&li->writeTime);
	measure_init(&li->readTime);
	li->write_op=li->read_op=li->trim_op=0;
	return NULL;
}

void *posix_destroy(lower_info *li){
	for(uint32_t i = 0; i < li->NOB; i++){
		if(seg_table[i].alloc){
			free(seg_table[i].storage);
		}
	}
	free(seg_table);
	pthread_mutex_destroy(&my_posix.lower_lock);
	pthread_mutex_destroy(&fd_lock);
#if (ASYNC==1)
	stopflag = true;
#endif
	return NULL;
}

void *posix_push_data(KEYT PPA, uint32_t size, value_set* value, bool async,algo_req *const req){
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		exit(1);
	}
	bench_lower_w_start(&my_posix);
	if(req->parents)
		bench_lower_start(req->parents);
	pthread_mutex_lock(&fd_lock);

	if(my_posix.SOP*PPA >= my_posix.TS){
		printf("\nwrite error\n");
		exit(2);
	}
	if(((lsm_params*)req->params)->lsm_type!=5){
#ifdef dftl
	uint8_t req_type = ((demand_params*)req->params)->type;
	if(req_type == 3 || req_type == 5 || req_type == 7){
#endif
#ifdef normal
	if(0){
#endif
		if(!seg_table[PPA/my_posix.PPS].alloc){
			seg_table[PPA/my_posix.PPS].storage = (PTR)malloc(my_posix.SOB);
			seg_table[PPA/my_posix.PPS].alloc = 1;
		}
		PTR loc = seg_table[PPA/my_posix.PPS].storage;
		memcpy(&loc[(PPA%my_posix.PPS)*my_posix.SOP],value->value,size);
	}

	pthread_mutex_unlock(&fd_lock);
	if(req->parents)
		bench_lower_end(req->parents);
	bench_lower_w_end(&my_posix);
	req->end_req(req);
	return NULL;
}

void *posix_pull_data(KEYT PPA, uint32_t size, value_set* value, bool async,algo_req *const req){	
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		exit(1);
	}
	bench_lower_r_start(&my_posix);
	if(req->parents)
		bench_lower_start(req->parents);

	pthread_mutex_lock(&fd_lock);

	if(my_posix.SOP*PPA >= my_posix.TS){
		printf("\nread error\n");
		exit(3);
	}
	if(((lsm_params*)req->params)->lsm_type!=4){
#ifdef dftl
	uint8_t req_type = ((demand_params*)req->params)->type;
	if(req_type == 2 || req_type == 4 || req_type == 6){
#endif
#ifdef normal
	if(0){
#endif
		PTR loc = seg_table[PPA/my_posix.PPS].storage;
		memcpy(value->value,&loc[(PPA%my_posix.PPS)*my_posix.SOP],size);
	}

	pthread_mutex_unlock(&fd_lock);

	if(req->parents)
		bench_lower_end(req->parents);
	bench_lower_r_end(&my_posix);
	req->end_req(req);
	/*
	if(async){
		req->end_req(req);
	}
	else{
	
	}*/
	return NULL;
}

void *posix_trim_block(KEYT PPA, bool async){
	bench_lower_t(&my_posix);
	char *temp=(char *)malloc(my_posix.SOB);
	memset(temp,0,my_posix.SOB);
	pthread_mutex_lock(&fd_lock);
	if(my_posix.SOP*PPA >= my_posix.TS || PPA%my_posix.PPS != 0){
		printf("\ntrim error\n");
		exit(4);
	}
	if(seg_table[PPA/my_posix.PPS].alloc){
		free(seg_table[PPA/my_posix.PPS].storage);
		seg_table[PPA/my_posix.PPS].storage = NULL;
		seg_table[PPA/my_posix.PPS].alloc = 0;
	}
	pthread_mutex_unlock(&fd_lock);
	free(temp);
	return NULL;
}

void posix_stop(){}