// ID_PM.C - Page Manager for SDL3 port
// Simplified implementation: loads entire VSWAP into RAM

#include "compat.h"
#include "id_heads.h"
#include "id_pm.h"
#include <stdint.h>

boolean XMSPresent, EMSPresent;
word XMSPagesAvail, EMSPagesAvail;

word ChunksInFile, PMSpriteStart, PMSoundStart;
PageListStruct *PMPages;

char PageFileName[13] = "VSWAP.";

static FILE *PageFile = NULL;
static byte *PageData = NULL;
static long PageDataSize = 0;

void PM_Startup(void) {
    char path[256];
    uint16_t tmp16;
    
    snprintf(path, sizeof(path), "wolf3d-data/%s", PageFileName);
    
    PageFile = fopen(path, "rb");
    if (!PageFile) {
        printf("PM_Startup: could not open %s\n", path);
        return;
    }
    
    // Read header (16-bit values in file)
    fread(&tmp16, sizeof(uint16_t), 1, PageFile); ChunksInFile = tmp16;
    fread(&tmp16, sizeof(uint16_t), 1, PageFile); PMSpriteStart = tmp16;
    fread(&tmp16, sizeof(uint16_t), 1, PageFile); PMSoundStart = tmp16;
    
    PMPages = malloc(ChunksInFile * sizeof(PageListStruct));
    
    // Read page offsets (32-bit values in file)
    uint32_t *offsets = malloc((ChunksInFile + 1) * sizeof(uint32_t));
    for (int i = 0; i <= ChunksInFile; i++) {
        fread(&offsets[i], sizeof(uint32_t), 1, PageFile);
    }
    
    // Calculate lengths
    for (int i = 0; i < ChunksInFile; i++) {
        PMPages[i].offset = offsets[i];
        PMPages[i].length = offsets[i + 1] - offsets[i];
        PMPages[i].locked = pml_Unlocked;
    }
    
    free(offsets);
    
    // Determine file size and load everything
    fseek(PageFile, 0, SEEK_END);
    PageDataSize = ftell(PageFile);
    fseek(PageFile, 0, SEEK_SET);
    
    PageData = malloc(PageDataSize);
    fread(PageData, 1, PageDataSize, PageFile);
    
    fclose(PageFile);
    PageFile = NULL;
}

void PM_Shutdown(void) {
    if (PageData) {
        free(PageData);
        PageData = NULL;
    }
    if (PMPages) {
        free(PMPages);
        PMPages = NULL;
    }
}

void PM_SetMainPurge(int level) {
}

void PM_SetMainMemPurge(int level) {
}

void PM_CheckMainMem(void) {
}

void PM_Preload(boolean (*update)(word current, word total)) {
}

memptr PM_GetPage(int page) {
    if (page < 0 || page >= ChunksInFile) return NULL;
    return PageData + PMPages[page].offset;
}

memptr PM_GetPageAddress(int page) {
    return PM_GetPage(page);
}

long PM_GetPageSize(int page) {
    if (page < 0 || page >= ChunksInFile) return 0;
    return PMPages[page].length;
}


