#ifndef __H_SETLSM__
#define __H_SETLSM__
#include "settings.h"

/*lsmtree structure*/
#define FLUSHNUM 1024
//#define MAXKEYINMETASEG (PAGESIZE/MINKEYLENGTH)
#define MAXKEYINMETASEG ((PAGESIZE-KEYBITMAP)/(DEFKEYLENGTH+4))
#ifdef KVSSD
#define KEYBITMAP 1024
#define TOMBSTONE (UINT32_MAX-2)
#define NOVALUE (UINT32_MAX-3)
#endif

#define RAF 1
#if LEVELN!=1
#define BLOOM
#endif

#define PREFIXNUM	(9)

//#define SIMDSEARCHER
//#define MULTILEVELREAD
//#define CACHEREORDER
#define PREFIXCHECK		4
#define PARTITION

//#define EMULATOR

#define DEFKEYINHEADER ((PAGESIZE-KEYBITMAP)/DEFKEYLENGTH)
//#define ONESEGMENT (DEFKEYINHEADER*DEFVALUESIZE)

#define KEYLEN(a) (a.len+sizeof(ppa_t)+1)
#define READCACHE
#define RANGEGETNUM 2
//#define USINGSLAB

//#define FASTFINDRUN
#define FASTFINDLOADFACTOR 0.5

#define NOEXTENDPPA(ppa) (ppa/NPCINPAGE)
/*lsmtree flash thread*/
#define KEYSETSIZE 8
#define CTHREAD 1
#define CQSIZE 128
#define FTHREAD 1
#define FQSIZE 2
#define RQSIZE 1024
#define WRITEWAIT
#define MAXKEYSIZE 255
#define THREADCOMPACTION 4
//#define CACHEFILETEST "cache_test_file.temp"

#define DELTACOMP	1
#define LZ4			2
#define COMPRESSEDCACHE DELTACOMP

typedef union data_info{
	ppa_t ppa;
	uint32_t v_len;
}data_info;

typedef struct map_entry{
	uint8_t type;
	KEYT key;
	data_info info;
	char *data;
}map_entry;
#define META_UNSEP

/*compaction*/
#define MAXITER 16
#define SPINLOCK
#endif
