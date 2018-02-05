#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/settings.h"
#include "../include/types.h"
#include "../bench/bench.h"
#include "interface.h"
int main(){
	int Input_cycle;
	int Input_type;
	int start;
	int end;
	int Input_size;

	printf("How many times would you run a benchmark?");
	scanf("%d", &Input_cycle);
	bench_init(Input_cycle);
	printf("please type the bench_type, start, end and input size\n");
	printf("====bench type list====\n");
	printf("SEQSET = 1, \nSEQGET = 2, \nRANDGET = 3, \nRANDRW = 4, \n");
	printf("SEQRW = 5, \nRANDSET = 6, \nMIXED = 7\n");
	printf("====bench type list end ====\n");
   printf("ex. 1 0 100 100 means seqset 0 to 100 with input size 100\n");
	for (int i = 0; i < Input_cycle; i++)
	{
		scanf("%d %d %d %d",&Input_type, &start, &end, &Input_size);
		if(Input_type == 1)
			bench_add(SEQSET,start,end,Input_size);
		else if(Input_type == 2)
			bench_add(SEQGET,start,end,Input_size);
		else if(Input_type == 3)
			bench_add(RANDGET,start,end,Input_size);
		else if(Input_type == 4)
			bench_add(RANDRW,start,end,Input_size);
		else if(Input_type == 5)
			bench_add(SEQRW,start,end,Input_size);
		else if(Input_type == 6)
			bench_add(RANDSET,start,end,Input_size);
		else if(Input_type == 7)
			bench_add(MIXED,start,end,Input_size);
		else{
			printf("invalid setting input is detected. please rerun the bench!\n");
			return 0;
		}
		printf("benchmark # %d is initiailized.\n",i+1);
		
		if( i == Input_cycle -1)
			printf("initilization done.\n");
		else
			printf("please type in next benchmark settings.\n");

	}

	printf("benchmark setting done. starts now.\n");

	inf_init();
	bench_value *value;
	while((value=get_bench())){
		char *data=(char*)malloc(PAGESIZE);
		memset(data,0,PAGESIZE);
		if(value->type==FS_SET_T){
			memcpy(data,&value->key,sizeof(value->key));
		}
#ifdef BENCH
		inf_make_req(value->type,value->key,data,value->mark);
#else
		inf_make_req(value->type,value->key,data);
#endif
	}
	
	while(!bench_is_finish()){
#ifdef LEAKCHECK
		sleep(1);
#endif
	}
	bench_print();
	bench_free();
	inf_free();
/*
	for(int i=0; i<1024*2; i++){
#ifdef LEAKCHECK
		printf("set: %d\n",i);
#endif
		int rand_key = rand()%10;
		key_save[i] = rand_key;
		printf("set: %d\n",rand_key);
		char *temp=(char*)malloc(PAGESIZE);
		memset(temp,0,PAGESIZE);
		memcpy(temp,&rand_key,sizeof(rand_key));
		inf_make_req(FS_SET_T,rand_key,temp);
	}
	for(int i=0; i<1024*2; i++){
		char *temp=(char*)malloc(PAGESIZE);
		memset(temp,0,PAGESIZE);
		inf_make_req(FS_GET_T,key_save[i],temp);
	}
 */
	return 0;
}
