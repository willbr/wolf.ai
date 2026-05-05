// ID_VL.C - SDL3 stub implementation

#include "ID_HEADS.H"

unsigned	bufferofs = 0;
unsigned	displayofs = 0, pelpan = 0;
unsigned	screenseg = 0;
unsigned	linewidth = 80;
unsigned	ylookup[MAXSCANLINES];
boolean		screenfaded = false;
unsigned	bordercolor = 0;

static byte	palette[768];
byte		gamepal[768];

void VL_Startup (void)
{
}

void VL_Shutdown (void)
{
}

void VL_SetVGAPlane (void)
{
}

void VL_SetTextMode (void)
{
}

void VL_DePlaneVGA (void)
{
}

void VL_SetVGAPlaneMode (void)
{
}

void VL_ClearVideo (byte color)
{
}

void VL_SetLineWidth (unsigned width)
{
	linewidth = width;
}

void VL_SetSplitScreen (int linenum)
{
}

void VL_WaitVBL (int vbls)
{
}

void VL_CrtcStart (int crtc)
{
}

void VL_SetScreen (int crtc, int pelpan)
{
}

void VL_FillPalette (int red, int green, int blue)
{
	int i;
	for (i = 0; i < 256; i++)
	{
		palette[i*3+0] = red;
		palette[i*3+1] = green;
		palette[i*3+2] = blue;
	}
}

void VL_SetColor (int color, int red, int green, int blue)
{
	palette[color*3+0] = red;
	palette[color*3+1] = green;
	palette[color*3+2] = blue;
}

void VL_GetColor (int color, int *red, int *green, int *blue)
{
	*red = palette[color*3+0];
	*green = palette[color*3+1];
	*blue = palette[color*3+2];
}

void VL_SetPalette (byte far *palette_src)
{
	_fmemcpy(palette, palette_src, 768);
}

void VL_GetPalette (byte far *palette_dest)
{
	_fmemcpy(palette_dest, palette, 768);
}

void VL_FadeOut (int start, int end, int red, int green, int blue, int steps)
{
	screenfaded = true;
}

void VL_FadeIn (int start, int end, byte far *palette, int steps)
{
	if (palette)
		VL_SetPalette(palette);
	screenfaded = false;
}

void VL_ColorBorder (int color)
{
	bordercolor = color;
}

void VL_Plot (int x, int y, int color)
{
}

void VL_Hlin (unsigned x, unsigned y, unsigned width, unsigned color)
{
}

void VL_Vlin (int x, int y, int height, int color)
{
}

void VL_Bar (int x, int y, int width, int height, int color)
{
}

void VL_DrawPicBare (int x, int y, byte far *pic, int width, int height)
{
}

void VL_MemToLatch (byte far *source, int width, int height, unsigned dest)
{
}

void VL_ScreenToScreen (unsigned source, unsigned dest, int width, int height)
{
}

void VL_MemToScreen (byte far *source, int width, int height, int x, int y)
{
}

void VL_MaskedToScreen (byte far *source, int width, int height, int x, int y)
{
}

void VL_LatchToScreen (unsigned source, int width, int height, int x, int y)
{
}

void VL_DrawTile8String (char *str, char far *tile8ptr, int printx, int printy)
{
}

void VL_DrawLatch8String (char *str, unsigned tile8ptr, int printx, int printy)
{
}

void VL_SizeTile8String (char *str, int *width, int *height)
{
	*width = 0;
	*height = 8;
}

void VL_DrawPropString (char *str, unsigned tile8ptr, int printx, int printy)
{
}

void VL_SizePropString (char *str, int *width, int *height, char far *font)
{
	*width = 0;
	*height = 0;
}

void VL_TestPaletteSet (void)
{
}

void VH_UpdateScreen (void)
{
}
