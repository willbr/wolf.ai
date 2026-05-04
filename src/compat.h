// compat.h - Compatibility layer for porting Borland C++ 3.0 DOS code to modern C

#ifndef COMPAT_H
#define COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Remove Borland memory model keywords
#define far
#define near
#define huge
#define __interrupt
#define _seg

// Remove Borland pragmas
#define hdrstop

// Segment:offset pointer macros (flat memory model)
#define MK_FP(seg, off) ((void *)((uintptr_t)(seg) * 16 + (off)))
#define FP_SEG(ptr) ((unsigned short)((uintptr_t)(ptr) >> 4))
#define FP_OFF(ptr) ((unsigned short)((uintptr_t)(ptr) & 0xF))

// Far memory functions -> standard functions
#define _fmemcpy(dst, src, n) memcpy(dst, src, n)
#define _fmemset(dst, c, n)   memset(dst, c, n)
#define _fmemcmp(a, b, n)     memcmp(a, b, n)
#define movedata(srcseg, srcoff, dstseg, dstoff, n) \
    memcpy(MK_FP(dstseg, dstoff), MK_FP(srcseg, srcoff), n)

// Memory allocation
#define farmalloc(size) malloc(size)
#define farfree(ptr)    free(ptr)
#define farcoreleft()   (1024 * 1024 * 1024)
#define coreleft()      (1024 * 1024)

// Interrupt and port I/O stubs
#define geninterrupt(n)
#define inportb(port)       0
#define outportb(port, val)
#define inport(port)        0
#define outport(port, val)
#define disable()
#define enable()
#define _segread(s)

// Register variables (stubs)
extern unsigned short _AX, _BX, _CX, _DX, _SI, _DI, _BP, _SP, _DS, _ES, _SS;

// Peek/poke
#define peekb(seg, off)     (*(unsigned char *)MK_FP(seg, off))
#define peek(seg, off)      (*(unsigned short *)MK_FP(seg, off))
#define pokeb(seg, off, v)  (*(unsigned char *)MK_FP(seg, off) = (v))
#define poke(seg, off, v)   (*(unsigned short *)MK_FP(seg, off) = (v))

// DOS-specific functions -> stubs or standard equivalents
#define _dos_getdiskfree(drive, dfree) 0
#define _dos_gettime(t) 0
#define _dos_getdate(d) 0
#define getvect(n)      NULL
#define setvect(n, f)

// Command line args (Borland globals)
extern int _argc;
extern char **_argv;

// DOS struct stubs
struct diskfree_t {
    unsigned total_clusters;
    unsigned avail_clusters;
    unsigned sectors_per_cluster;
    unsigned bytes_per_sector;
};
struct dostime_t {
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
    unsigned char hsecond;
};
struct dosdate_t {
    unsigned char day;
    unsigned char month;
    unsigned int year;
    unsigned char dayofweek;
};

// File I/O compat
#ifndef O_BINARY
#define O_BINARY        0
#endif
#define _open           open
#define _read           read
#define _write          write
#define _close          close
#define _lseek          lseek
#define _filelength     filelength
#define _unlink         unlink

// <VALUES.h> replacements
#define MAXINT          INT_MAX
#define MAXLONG         LONG_MAX
#define MAXSHORT        SHRT_MAX
#define HIBITS          (-MAXSHORT)
#define HIBITL          (-MAXLONG)
#define MAXDOUBLE       DBL_MAX
#define MINDOUBLE       DBL_MIN

// <DIR.h> replacements
#define MAXPATH         260
#define MAXDRIVE        3
#define MAXDIR          256
#define MAXFILE         256
#define MAXEXT          256

// Far string functions -> standard functions
#define _fstrcpy(dst, src)      strcpy((char *)(dst), (const char *)(src))
#define _fstrncpy(dst, src, n)  strncpy((char *)(dst), (const char *)(src), n)
#define _fstrcat(dst, src)      strcat((char *)(dst), (const char *)(src))
#define _fstrcmp(a, b)          strcmp((const char *)(a), (const char *)(b))
#define _fstrlen(s)             strlen((const char *)(s))

// DOS file I/O stubs
#define _dos_write(handle, buf, count, written) \
    do { *(written) = _write((handle), (buf), (count)); } while(0)
#define _dos_read(handle, buf, count, read) \
    do { *(read) = _read((handle), (buf), (count)); } while(0)
#define _dos_open(path, oflag, handle) \
    do { *(handle) = _open((path), (oflag)); } while(0)
#define _dos_close(handle)      _close(handle)

// Borland conio functions -> stubs or equivalents
#define gotoxy(x, y)
#define clrscr()
#define wherex()        0
#define wherey()        0
#define kbhit()         0
#define getch()         0
#define getche()        0
#define putch(c)
#define textcolor(c)
#define textbackground(c)
#define cprintf         printf
#define cputs(s)        fputs(s, stdout)

// Delay function (will be defined in sdl3 layer)
extern void SDL3_Delay(unsigned int ms);
#define delay(ms)       SDL3_Delay(ms)

// Misc
#define random(num)     (rand() % (num))
#define randomize()     srand((unsigned)time(NULL))

// filelength for standard file descriptors
static inline long wolf_filelength(int fd) {
    struct stat st;
    if (fstat(fd, &st) == 0) return (long)st.st_size;
    return -1;
}
#define filelength(fd)  wolf_filelength(fd)

#endif // COMPAT_H
