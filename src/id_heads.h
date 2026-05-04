// ID_GLOB.H - Modified for SDL3 port

#ifndef __ID_GLOB__
#define __ID_GLOB__

#include "compat.h"
#include "version.h"

//--------------------------------------------------------------------------

extern char signon;

#define introscn signon

#ifdef JAPAN
#ifdef JAPDEMO
#include "gfxv_wj1.h"
#else
#include "gfxv_wj6.h"
#endif
#include "audiowl6.h"
#include "mapswl6.h"
#else

#ifndef SPEAR

#include "gfxv_wl6.h"
#include "audiowl6.h"
#include "mapswl6.h"

#else

#ifndef SPEARDEMO
#include "gfxv_sod.h"
#include "audiosod.h"
#include "mapssod.h"
#else
#include "gfxv_sdm.h"
#include "audiosdm.h"
#include "mapssdm.h"
#endif

#endif
#endif
//-----------------


#define GREXT	"VGA"

//
//	ID Engine
//	Types.h - Generic types, #defines, etc.
//	v1.0d1
//

typedef bool boolean;
typedef unsigned char		byte;
typedef unsigned int			word;
typedef unsigned long		longword;
typedef byte *					Ptr;

typedef struct
		{
			int x,y;
		} Point;
typedef struct
		{
			Point ul,lr;
		} Rect;

#define nil ((void *)0)


#include "id_mm.h"
#include "id_pm.h"
#include "id_ca.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_in.h"
#include "id_sd.h"
#include "id_us.h"


void	Quit (char *error);		// defined in user program

//
// replacing refresh manager with custom routines
//

#define	PORTTILESWIDE		20      // all drawing takes place inside a
#define	PORTTILESHIGH		13		// non displayed port of this size

#define	UPDATEWIDE			PORTTILESWIDE
#define	UPDATEHIGH			PORTTILESHIGH

#define	MAXTICS				10
#define	DEMOTICS			4

#define	UPDATETERMINATE	0x0301

extern	unsigned	mapwidth,mapheight,tics;
extern	boolean		compatability;

extern	byte		*updateptr;
extern	unsigned	uwidthtable[UPDATEHIGH];
extern	unsigned	blockstarts[UPDATEWIDE*UPDATEHIGH];

extern	byte		fontcolor,backcolor;

#define SETFONTCOLOR(f,b) fontcolor=f;backcolor=b;

#endif // __ID_GLOB__
