// ID_MM.H - Memory Manager Header

#ifndef __ID_MM__
#define __ID_MM__

#define SAVENEARHEAP	0x400
#define SAVEFARHEAP		0
#define BUFFERSIZE		0x1000
#define MAXBLOCKS		700

// Compatibility macros for flat memory
#define MM_GetPtr(baseptr, size) _MM_GetPtr((void **)(baseptr), size)
#define MM_FreePtr(baseptr) _MM_FreePtr((void **)(baseptr))
#define MM_SetPurge(baseptr, purge) _MM_SetPurge((void **)(baseptr), purge)
#define MM_SetLock(baseptr, locked) _MM_SetLock((void **)(baseptr), locked)

typedef void * memptr;

typedef struct
{
	long	nearheap,farheap,EMSmem,XMSmem,mainmem;
} mminfotype;

extern	mminfotype	mminfo;
extern	memptr		bufferseg;
extern	boolean		mmerror;

extern	void		(* beforesort) (void);
extern	void		(* aftersort) (void);

void MM_Startup (void);
void MM_Shutdown (void);
void MM_MapEMS (void);

void _MM_GetPtr (void **baseptr, unsigned long size);
void _MM_FreePtr (void **baseptr);
void _MM_SetPurge (void **baseptr, int purge);
void _MM_SetLock (void **baseptr, boolean locked);
void MM_SortMem (void);

void MM_ShowMemory (void);

long MM_UnusedMemory (void);
long MM_TotalFree (void);

void MM_BombOnError (boolean bomb);

void MML_UseSpace (unsigned segstart, unsigned seglength);

#endif
