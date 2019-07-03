#include "variable.h"
#include "lsmtree.h"
#include "level.h"
#include "page.h"
#include <stdlib.h>
#include <stdio.h>
extern lsmtree LSM;
void *variable_value2Page(level *in, l_bucket *src, value_set ***target_valueset, int* target_valueset_from, bool isgc){
	int v_idx;
	/*for normal data*/
	value_set **v_des=NULL;

	/*for gc*/
	htable_t *table_data;
	uint32_t target_ppa;

	v_idx=*target_valueset_from;
	if(isgc){/*v_idx for gc_container*/
	//	gc_container=*((gc_node***)target_valueset);
	}
	else{/*v_idx for value_set*/
		v_des=*target_valueset;
	}

	uint8_t max_piece=PAGESIZE/PIECE-1; //the max_piece is wrote before enter this section
	while(src->idx[max_piece]==0 && max_piece>0) --max_piece;

//	bool debuging=false;
	while(max_piece){
		PTR page=NULL;
		int ptr=0;
		int remain=PAGESIZE;
	
		if(isgc){
			table_data=(htable_t*)malloc(sizeof(htable_t));
			page=(PTR)table_data->sets;
			target_ppa=LSM.lop->moveTo_fr_page(true);
		}else{
			v_des[v_idx]=inf_get_valueset(page,FS_MALLOC_W,PAGESIZE);
			v_des[v_idx]->ppa=LSM.lop->moveTo_fr_page(false);
			page=v_des[v_idx]->value;
			target_ppa=v_des[v_idx]->ppa;
		}

		footer *foot=(footer*)pm_get_oob(target_ppa,DATA);
		//footer *foot=(footer*)calloc(sizeof(footer),1);
		uint8_t used_piece=0;
		while(remain>0){
			int target_length=(remain/PIECE>max_piece?max_piece:remain/PIECE);
			while(src->idx[target_length]==0 && target_length!=0) --target_length;
			if(target_length==0){
				break;
			}
			if(isgc){
				gc_node *target=src->gc_bucket[target_length][src->idx[target_length]-1];
				target->nppa=LSM.lop->get_page(target->plength);
				foot->map[target->nppa%NPCINPAGE]=target_length;
				validate_PPA(DATA,target->nppa);

				memcpy(&page[ptr],target->value,target_length*PIECE);
			}else{
				snode *target=src->bucket[target_length][src->idx[target_length]-1];
				target->ppa=LSM.lop->get_page(target->value->length);
				foot->map[target->ppa%NPCINPAGE]=target->value->length;
				validate_PPA(DATA,target->ppa);
				memcpy(&page[ptr],target->value->value,target_length*PIECE);
			}
			used_piece+=target_length;
			src->idx[target_length]--;

			ptr+=target_length*PIECE;
			remain-=target_length*PIECE;
		}

		if(isgc){
			gc_data_write(target_ppa,table_data,true);
			free(table_data);
		}
		else{	
			v_idx++;
		}

	//	free(foot);	
		bool stop=0;
		for(int i=0; i<PAGESIZE/PIECE+1; i++){
			if(src->idx[i]!=0)
				break;
			if(i==PAGESIZE/PIECE) stop=true;
		}
		if(stop) break;
	}
	*target_valueset_from=v_idx;
	return v_des;
}