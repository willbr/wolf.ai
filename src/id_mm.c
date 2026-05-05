// ID_MM.C - Memory Manager for SDL3 port
// Simplified flat-memory implementation using malloc/free

#include "ID_HEADS.H"
#include "id_mm.h"

mminfotype mminfo;
memptr bufferseg;
boolean mmerror;

void (*beforesort)(void);
void (*aftersort)(void);

static struct {
    void *ptr;
    unsigned long size;
    int purge;
    boolean locked;
} blocks[MAXBLOCKS];
static int num_blocks = 0;

void MM_Startup(void) {
    memset(&mminfo, 0, sizeof(mminfo));
    mminfo.nearheap = 1024 * 1024;
    mminfo.farheap = 1024 * 1024;
    mminfo.mainmem = 16 * 1024 * 1024;
    mmerror = false;
    num_blocks = 0;
    bufferseg = malloc(BUFFERSIZE);
}

void MM_Shutdown(void) {
    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].ptr) {
            free(blocks[i].ptr);
            blocks[i].ptr = NULL;
        }
    }
    num_blocks = 0;
    if (bufferseg) {
        free(bufferseg);
        bufferseg = NULL;
    }
}

void _MM_GetPtr(void **baseptr, unsigned long size) {
    if (!baseptr) return;
    if (*baseptr) {
        _MM_FreePtr(baseptr);
    }
    *baseptr = malloc(size);
    if (!*baseptr) {
        mmerror = true;
        return;
    }
    memset(*baseptr, 0, size);
    if (num_blocks < MAXBLOCKS) {
        blocks[num_blocks].ptr = *baseptr;
        blocks[num_blocks].size = size;
        blocks[num_blocks].purge = 0;
        blocks[num_blocks].locked = false;
        num_blocks++;
    }
}

void _MM_FreePtr(void **baseptr) {
    if (!baseptr || !*baseptr) return;
    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].ptr == *baseptr) {
            free(blocks[i].ptr);
            blocks[i] = blocks[num_blocks - 1];
            num_blocks--;
            *baseptr = NULL;
            return;
        }
    }
    free(*baseptr);
    *baseptr = NULL;
}

void _MM_SetPurge(void **baseptr, int purge) {
    if (!baseptr || !*baseptr) return;
    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].ptr == *baseptr) {
            blocks[i].purge = purge;
            return;
        }
    }
}

void _MM_SetLock(void **baseptr, boolean locked) {
    if (!baseptr || !*baseptr) return;
    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].ptr == *baseptr) {
            blocks[i].locked = locked;
            return;
        }
    }
}

void MM_SortMem(void) {
}

void MM_ShowMemory(void) {
    printf("Memory: %d blocks allocated\n", num_blocks);
}

long MM_UnusedMemory(void) {
    return 8 * 1024 * 1024;
}

long MM_TotalFree(void) {
    return 16 * 1024 * 1024;
}

void MM_BombOnError(boolean bomb) {
    (void)bomb;
}

void MML_UseSpace(unsigned segstart, unsigned seglength) {
    (void)segstart;
    (void)seglength;
}

void MM_MapEMS(void) {
}
