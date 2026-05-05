// ID_CA.C - Cache Manager for SDL3 port
// Portable C implementation with flat memory

#include "compat.h"
#include "id_heads.h"
#include "id_ca.h"

char audioname[13] = "AUDIO.";

byte *tinf;
int mapon;

unsigned *mapsegs[MAPPLANES];
maptype *mapheaderseg[NUMMAPS];
byte *audiosegs[NUMSNDCHUNKS];
void *grsegs[NUMCHUNKS];

byte grneeded[NUMCHUNKS];
byte ca_levelbit, ca_levelnum;

char *titleptr[8];

int profilehandle, debughandle;

char extension[5] = "WL1";
char gheadname[16] = "VGAHEAD.WL1";
char gfilename[16] = "VGAGRAPH.WL1";
char gdictname[16] = "VGADICT.WL1";
char mheadname[16] = "MAPHEAD.WL1";
char mfilename[16] = "GAMEMAPS.WL1";
char aheadname[16] = "AUDIOHED.WL1";
char afilename[16] = "AUDIOT.WL1";

byte *grstarts;
long *audiostarts;
int numgrchunks = 0;

#define THREEBYTEGRSTARTS

#ifdef THREEBYTEGRSTARTS
static long GRFILEPOS(int c) {
    long value;
    int offset = c * 3;
    value = grstarts[offset] | (grstarts[offset + 1] << 8) | (grstarts[offset + 2] << 16);
    if (value == 0xffffffl)
        value = -1;
    return value;
}
#else
#define GRFILEPOS(c) (((long *)grstarts)[c])
#endif

void (*drawcachebox)(char *title, unsigned numcache);
void (*updatecachebox)(void);
void (*finishcachebox)(void);

// Huffman decoding
#define NEARTAG 0xA7
#define FARTAG  0x3D

typedef struct {
    uint16_t bit0, bit1;  // 0-255 is a character, >=256 is a node index+256
} huffnode;

static huffnode *grhuffman;
static huffnode *audiohuffman;

static int grhandle = -1;
static int maphandle = -1;
static int audiohandle = -1;

//
// Carmack expansion
//
void CAL_CarmackExpand(unsigned short *source, unsigned short *dest, unsigned length) {
    unsigned short ch, chhigh, count, offset;
    unsigned short *copyptr, *inptr, *outptr;
    
    inptr = source;
    outptr = dest;
    
    while (length > 0) {
        ch = *inptr++;
        chhigh = ch >> 8;
        
        if (chhigh == NEARTAG || chhigh == FARTAG) {
            count = ch & 0xFF;
            if (!count) {
                *outptr++ = ch;
                length--;
            } else {
                offset = *inptr++;
                if (chhigh == NEARTAG) {
                    copyptr = outptr - offset;
                } else {
                    copyptr = dest + offset;
                }
                while (count-- && length > 0) {
                    *outptr++ = *copyptr++;
                    length--;
                }
            }
        } else {
            *outptr++ = ch;
            length--;
        }
    }
}

//
// RLEW expansion
//
void CA_RLEWexpand(unsigned *source, unsigned *dest, long length, unsigned rlewtag) {
    unsigned value, count;
    unsigned *inptr = source;
    unsigned *outptr = dest;
    long expanded = 0;
    
    while (expanded < length) {
        value = *inptr++;
        if (value == rlewtag) {
            count = *inptr++;
            value = *inptr++;
            while (count-- && expanded < length) {
                *outptr++ = value;
                expanded++;
            }
        } else {
            *outptr++ = value;
            expanded++;
        }
    }
}

//
// Huffman expansion
//
void CAL_HuffExpand(byte *source, byte *dest, long length, huffnode *hufftable) {
    byte *end = dest + length;
    unsigned bit = 0;
    byte val = *source++;
    int node = 254;  // head node is always node 254
    
    while (dest < end) {
        uint16_t branch;
        
        if ((val >> bit) & 1)
            branch = hufftable[node].bit1;
        else
            branch = hufftable[node].bit0;
        
        bit++;
        if (bit == 8) {
            val = *source++;
            bit = 0;
        }
        
        if (branch < 256) {
            *dest++ = (byte)branch;
            node = 254;  // back to head node
        } else {
            node = branch - 256;
        }
    }
}

//
// File I/O helpers
//
boolean CA_FarRead(int handle, byte *dest, long length) {
    return read(handle, dest, length) == length;
}

boolean CA_FarWrite(int handle, byte *source, long length) {
    return write(handle, source, length) == length;
}

boolean CA_ReadFile(char *filename, memptr *ptr) {
    int handle;
    long length;
    
    handle = open(filename, O_RDONLY | O_BINARY);
    if (handle == -1) return false;
    
    length = filelength(handle);
    MM_GetPtr(ptr, length);
    if (!CA_FarRead(handle, *ptr, length)) {
        MM_FreePtr(ptr);
        close(handle);
        return false;
    }
    close(handle);
    return true;
}

boolean CA_LoadFile(char *filename, memptr *ptr) {
    return CA_ReadFile(filename, ptr);
}

boolean CA_WriteFile(char *filename, void *ptr, long length) {
    int handle = open(filename, O_CREAT | O_WRONLY | O_BINARY, S_IREAD | S_IWRITE);
    if (handle == -1) return false;
    boolean result = CA_FarWrite(handle, ptr, length);
    close(handle);
    return result;
}

//
// Startup / Shutdown
//
void CA_Startup(void) {
    char path[256];
    int handle;
    long length;
    
    snprintf(path, sizeof(path), "wolf3d-data/%s", gdictname);
    handle = open(path, O_RDONLY | O_BINARY);
    if (handle != -1) {
        length = filelength(handle);
        grhuffman = malloc(length);
        read(handle, grhuffman, length);
        close(handle);
    }
    
    snprintf(path, sizeof(path), "wolf3d-data/%s", gheadname);
    handle = open(path, O_RDONLY | O_BINARY);
    if (handle != -1) {
        length = filelength(handle);
        MM_GetPtr((memptr *)&grstarts, length);
        read(handle, grstarts, length);
        close(handle);
        numgrchunks = length / 3 - 1;
    }
    
    snprintf(path, sizeof(path), "wolf3d-data/%s", aheadname);
    handle = open(path, O_RDONLY | O_BINARY);
    if (handle != -1) {
        length = filelength(handle);
        MM_GetPtr((memptr *)&audiostarts, length);
        read(handle, audiostarts, length);
        close(handle);
    }
    
    snprintf(path, sizeof(path), "wolf3d-data/%s", mfilename);
    maphandle = open(path, O_RDONLY | O_BINARY);
    
    snprintf(path, sizeof(path), "wolf3d-data/%s", gfilename);
    grhandle = open(path, O_RDONLY | O_BINARY);
    
    snprintf(path, sizeof(path), "wolf3d-data/%s", afilename);
    audiohandle = open(path, O_RDONLY | O_BINARY);
    
    // Load pictable from graphics file chunk 0
    if (grhandle != -1 && grstarts && numgrchunks > 1) {
        long pos = GRFILEPOS(0);
        long compressed = GRFILEPOS(1) - pos;
        if (compressed > 0) {
            byte *source = malloc(compressed);
            lseek(grhandle, pos, SEEK_SET);
            read(grhandle, source, compressed);
            long expanded = *(long *)source;
            MM_GetPtr((memptr *)&pictable, expanded);
            CAL_HuffExpand(source + 4, (byte *)pictable, expanded, grhuffman);
            free(source);
        }
    }
}

void CA_Shutdown(void) {
    if (grhuffman) { free(grhuffman); grhuffman = NULL; }
    if (audiohuffman) { free(audiohuffman); audiohuffman = NULL; }
    if (grstarts) { MM_FreePtr((memptr *)&grstarts); }
    if (audiostarts) { MM_FreePtr((memptr *)&audiostarts); }
    if (maphandle != -1) { close(maphandle); maphandle = -1; }
    if (grhandle != -1) { close(grhandle); grhandle = -1; }
    if (audiohandle != -1) { close(audiohandle); audiohandle = -1; }
}

void CA_SetGrPurge(void) {
    for (int i = 0; i < NUMCHUNKS; i++) {
        if (grsegs[i]) {
            MM_SetPurge((memptr *)&grsegs[i], 3);
        }
    }
}

void CA_CacheAudioChunk(int chunk) {
    if (!audiosegs[chunk]) {
        long offset = audiostarts[chunk];
        long length = audiostarts[chunk + 1] - offset;
        if (length < 0 || length > 1000000) {
            return;
        }
        MM_GetPtr((memptr *)&audiosegs[chunk], length);
        lseek(audiohandle, offset, SEEK_SET);
        read(audiohandle, audiosegs[chunk], length);
    }
}

void CA_LoadAllSounds(void) {
    for (int i = 0; i < NUMSNDCHUNKS; i++) {
        CA_CacheAudioChunk(i);
    }
}

void CAL_ExpandGrChunk(int chunk, byte *source) {
    long expanded;
    
    if (chunk >= STARTTILE8 && chunk < STARTEXTERNS) {
        // Tile chunks have implicit sizes
        if (chunk < STARTTILE8M)
            expanded = 64 * NUMTILE8;
        else if (chunk < STARTTILE16)
            expanded = 128 * NUMTILE8M;
        else if (chunk < STARTTILE16M)
            expanded = 64 * 4;
        else if (chunk < STARTTILE32)
            expanded = 128 * 4;
        else if (chunk < STARTTILE32M)
            expanded = 64 * 16;
        else
            expanded = 128 * 16;
    } else {
        // Everything else has an explicit size longword
        expanded = *(long *)source;
        source += 4;
    }
    
    MM_GetPtr((memptr *)&grsegs[chunk], expanded);
    if (mmerror) return;
    
    CAL_HuffExpand(source, (byte *)grsegs[chunk], expanded, grhuffman);
}

void CA_CacheGrChunk(int chunk) {
    long pos, compressed;
    byte *source;
    int next;
    
    if (chunk < 0 || chunk >= NUMCHUNKS) return;
    
    grneeded[chunk] |= ca_levelbit;
    if (grsegs[chunk]) {
        MM_SetPurge((memptr *)&grsegs[chunk], 0);
        return;
    }
    
    pos = GRFILEPOS(chunk);
    if (pos < 0)  // sparse tile
        return;
    
    next = chunk + 1;
    while (GRFILEPOS(next) == -1)
        next++;
    
    compressed = GRFILEPOS(next) - pos;
    
    lseek(grhandle, pos, SEEK_SET);
    
    source = malloc(compressed);
    read(grhandle, source, compressed);
    
    CAL_ExpandGrChunk(chunk, source);
    free(source);
}

void CA_CacheMap(int mapnum) {
    if (mapheaderseg[mapnum]) return;
    
    MM_GetPtr((memptr *)&mapheaderseg[mapnum], sizeof(maptype));
    
    for (int plane = 0; plane < MAPPLANES; plane++) {
        MM_GetPtr((memptr *)&mapsegs[plane], 64 * 64 * 2);
    }
}

void CA_CacheMarks(void) {
    for (int i = 0; i < NUMCHUNKS; i++) {
        if (grneeded[i] & ca_levelbit) {
            CA_CacheGrChunk(i);
        }
    }
}

void CA_ClearMarks(void) {
    memset(grneeded, 0, sizeof(grneeded));
}

void CA_ClearAllMarks(void) {
    memset(grneeded, 0, sizeof(grneeded));
}

void CA_UpLevel(void) {
    ca_levelbit <<= 1;
    ca_levelnum++;
}

void CA_DownLevel(void) {
    ca_levelbit >>= 1;
    ca_levelnum--;
}

void CA_SetAllPurge(void) {
    CA_SetGrPurge();
}

void CA_OpenDebug(void) {
    debughandle = open("DEBUG.TXT", O_CREAT | O_WRONLY | O_TRUNC);
}

void CA_CloseDebug(void) {
    if (debughandle != -1) {
        close(debughandle);
        debughandle = -1;
    }
}

void CA_CacheScreen(int chunk) {
    long pos, compressed, expanded;
    byte *source, *dest;
    int next;
    
    pos = GRFILEPOS(chunk);
    if (pos < 0) return;
    
    next = chunk + 1;
    while (GRFILEPOS(next) == -1)
        next++;
    compressed = GRFILEPOS(next) - pos;
    
    lseek(grhandle, pos, SEEK_SET);
    source = malloc(compressed);
    read(grhandle, source, compressed);
    
    expanded = *(long *)source;
    dest = malloc(expanded);
    CAL_HuffExpand(source + 4, dest, expanded, grhuffman);
    
    VL_CopyPlanarToScreen(dest, expanded);
    VW_MarkUpdateBlock(0, 0, 319, 199);
    
    free(source);
    free(dest);
}

long CA_RLEWCompress(unsigned *source, long length, unsigned *dest, unsigned rlewtag) {
    (void)source;
    (void)length;
    (void)dest;
    (void)rlewtag;
    return 0;
}

void CAL_ShiftSprite(unsigned segment, unsigned source, unsigned dest,
    unsigned width, unsigned height, unsigned pixshift) {
    (void)segment;
    (void)source;
    (void)dest;
    (void)width;
    (void)height;
    (void)pixshift;
}
