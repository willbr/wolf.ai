// ID_VL.C - SDL3 Video Backend Implementation
// Emulates VGA Mode Y (planar 256-color) in software

#include "ID_HEADS.H"
#include <SDL3/SDL.h>

void VH_UpdateScreen(void);

unsigned	bufferofs = 0;
unsigned	displayofs = 0, pelpan = 0;
unsigned	screenseg = 0;
unsigned	linewidth = 80;
unsigned	ylookup[MAXSCANLINES];
boolean		screenfaded = false;
unsigned	bordercolor = 0;

static byte	palette[768];
byte		gamepal[768];

// Planar screen buffer: 4 planes x 3 pages x 16000 addresses
// Mode Y: each address holds one pixel in one plane
// Plane P at address A corresponds to pixel ((A % 80) * 4 + P, A / 80)
#define PAGE_SIZE	16000	// 80 bytes/line * 200 lines
#define NUM_PAGES	3
// Must be at least FREESTART (SCREENSIZE*3 = 16640*3 = 49920)
byte	screenbuffer[4][50000];

// SDL globals
static SDL_Window	*sdl_window = NULL;
static SDL_Renderer	*sdl_renderer = NULL;
static SDL_Texture	*sdl_texture = NULL;
static Uint32		*sdl_pixels = NULL;
static int			sdl_pitch = 0;

// Linear staging buffer for SDL (320x200, 32-bit RGBA)
static Uint32		linear_buffer[320 * 200];

//
// Convert planar screen buffer to linear RGB buffer
//
static void PlanarToLinear(void)
{
    int y, x;
    byte r, g, b;

    for (y = 0; y < 200; y++) {
        int row_offset = y * 80;
        int linear_row = y * 320;
        for (x = 0; x < 320; x++) {
            int plane = x & 3;
            int addr = row_offset + (x >> 2);
            byte c = screenbuffer[plane][bufferofs + addr];
            r = palette[c * 3 + 0] * 255 / 63;
            g = palette[c * 3 + 1] * 255 / 63;
            b = palette[c * 3 + 2] * 255 / 63;
            linear_buffer[linear_row + x] = (0xFF000000) | (r << 16) | (g << 8) | b;
        }
    }
}

//
// Initialize SDL video
//
void VL_Startup(void)
{
    int i;

    // Initialize ylookup table for planar addressing
    for (i = 0; i < MAXSCANLINES; i++)
        ylookup[i] = i * 80;

    // Create SDL window (640x400 for 2x scaling, or 1280x800 for 4x)
    sdl_window = SDL_CreateWindow("Wolfenstein 3D",
        640, 400,
        SDL_WINDOW_RESIZABLE);
    if (!sdl_window) {
        fprintf(stderr, "VL_Startup: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }

    sdl_renderer = SDL_CreateRenderer(sdl_window, NULL);
    if (!sdl_renderer) {
        fprintf(stderr, "VL_Startup: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return;
    }

    SDL_SetRenderLogicalPresentation(sdl_renderer, 320, 200,
        SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    sdl_texture = SDL_CreateTexture(sdl_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        320, 200);
    if (!sdl_texture) {
        fprintf(stderr, "VL_Startup: SDL_CreateTexture failed: %s\n", SDL_GetError());
        return;
    }

    // Clear screen buffers
    memset(screenbuffer, 0, sizeof(screenbuffer));

    // Default palette (grayscale)
    for (i = 0; i < 256; i++) {
        palette[i*3+0] = i >> 2;
        palette[i*3+1] = i >> 2;
        palette[i*3+2] = i >> 2;
    }

    // Load game palette
    {
        FILE *pf = fopen("wolf3d-data/gamepal.bin", "rb");
        if (pf) {
            fread(gamepal, 1, 768, pf);
            fclose(pf);
        }
    }
}

void VL_Shutdown(void)
{
    if (sdl_texture) {
        SDL_DestroyTexture(sdl_texture);
        sdl_texture = NULL;
    }
    if (sdl_renderer) {
        SDL_DestroyRenderer(sdl_renderer);
        sdl_renderer = NULL;
    }
    if (sdl_window) {
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
    }
}

void VL_SetVGAPlane(void)
{
}

void VL_SetTextMode(void)
{
}

void VL_DePlaneVGA(void)
{
}

void VL_SetVGAPlaneMode(void)
{
}

void VL_ClearVideo(byte color)
{
    int plane;
    for (plane = 0; plane < 4; plane++)
        memset(screenbuffer[plane], color, sizeof(screenbuffer[plane]));
}

void VL_SetLineWidth(unsigned width)
{
    linewidth = width;
}

void VL_SetSplitScreen(int linenum)
{
    (void)linenum;
}

void VL_WaitVBL(int vbls)
{
    (void)vbls;
    SDL_Delay((Uint32)(vbls * 14));
}

void VL_CrtcStart(int crtc)
{
    (void)crtc;
}

void VL_SetScreen(int crtc, int pelpan)
{
    (void)crtc;
    (void)pelpan;
}

void VL_FillPalette(int red, int green, int blue)
{
    int i;
    for (i = 0; i < 256; i++) {
        palette[i*3+0] = red;
        palette[i*3+1] = green;
        palette[i*3+2] = blue;
    }
}

void VL_SetColor(int color, int red, int green, int blue)
{
    palette[color*3+0] = red;
    palette[color*3+1] = green;
    palette[color*3+2] = blue;
}

void VL_GetColor(int color, int *red, int *green, int *blue)
{
    *red = palette[color*3+0];
    *green = palette[color*3+1];
    *blue = palette[color*3+2];
}

void VL_SetPalette(byte far *palette_src)
{
    _fmemcpy(palette, palette_src, 768);
}

void VL_GetPalette(byte far *palette_dest)
{
    _fmemcpy(palette_dest, palette, 768);
}

void VL_FadeOut(int start, int end, int red, int green, int blue, int steps)
{
    int i, j;
    byte origpal[768];
    byte newpal[768];

    (void)start;
    (void)end;

    _fmemcpy(origpal, palette, 768);

    for (i = 0; i < steps; i++) {
        for (j = 0; j < 256; j++) {
            newpal[j*3+0] = origpal[j*3+0] + (red - origpal[j*3+0]) * i / steps;
            newpal[j*3+1] = origpal[j*3+1] + (green - origpal[j*3+1]) * i / steps;
            newpal[j*3+2] = origpal[j*3+2] + (blue - origpal[j*3+2]) * i / steps;
        }
        _fmemcpy(palette, newpal, 768);
        VH_UpdateScreen();
        SDL_Delay(14);
    }

    VL_FillPalette(red, green, blue);
    VH_UpdateScreen();
    screenfaded = true;
}

void VL_FadeIn(int start, int end, byte far *palette_src, int steps)
{
    int i, j;
    byte newpal[768];

    (void)start;
    (void)end;

    for (i = 0; i < steps; i++) {
        for (j = 0; j < 256; j++) {
            newpal[j*3+0] = palette_src[j*3+0] * i / steps;
            newpal[j*3+1] = palette_src[j*3+1] * i / steps;
            newpal[j*3+2] = palette_src[j*3+2] * i / steps;
        }
        _fmemcpy(palette, newpal, 768);
        VH_UpdateScreen();
        SDL_Delay(14);
    }

    _fmemcpy(palette, palette_src, 768);
    VH_UpdateScreen();
    screenfaded = false;
}

void VL_ColorBorder(int color)
{
    bordercolor = color;
}

void VL_Plot(int x, int y, int color)
{
    int plane, addr;
    if (x < 0 || x >= 320 || y < 0 || y >= 200)
        return;
    plane = x & 3;
    addr = ylookup[y] + (x >> 2);
    screenbuffer[plane][bufferofs + addr] = (byte)color;
}

void VL_Hlin(unsigned x, unsigned y, unsigned width, unsigned color)
{
    unsigned i;
    for (i = 0; i < width; i++)
        VL_Plot(x + i, y, color);
}

void VL_Vlin(int x, int y, int height, int color)
{
    int i;
    for (i = 0; i < height; i++)
        VL_Plot(x, y + i, color);
}

void VL_Bar(int x, int y, int width, int height, int color)
{
    int row, col;
    int plane, addr;

    // Clip to screen bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 320) width = 320 - x;
    if (y + height > 200) height = 200 - y;
    if (width <= 0 || height <= 0) return;

    for (row = 0; row < height; row++) {
        int base_addr = bufferofs + ylookup[y + row];
        // Handle left partial byte
        int start_plane = x & 3;
        int end_x = x + width;

        for (col = x; col < end_x; col++) {
            plane = col & 3;
            addr = base_addr + (col >> 2);
            screenbuffer[plane][addr] = (byte)color;
        }
    }
}

void VL_DrawPicBare(int x, int y, byte far *pic, int width, int height)
{
    VL_MemToScreen(pic, width, height, x, y);
}

void VL_MemToLatch(byte far *source, int width, int height, unsigned dest)
{
    (void)source;
    (void)width;
    (void)height;
    (void)dest;
}

void VL_ScreenToScreen(unsigned source, unsigned dest, int width, int height)
{
    int plane, row, col;
    int w = width >> 2;
    int src_base, dst_base;

    for (plane = 0; plane < 4; plane++) {
        for (row = 0; row < height; row++) {
            src_base = source + ylookup[row];
            dst_base = dest + ylookup[row];
            for (col = 0; col < w; col++) {
                screenbuffer[plane][dst_base + col] = screenbuffer[plane][src_base + col];
            }
        }
    }
}

void VL_MemToScreen(byte far *source, int width, int height, int x, int y)
{
    int plane, row, col;
    int src_row_bytes = width >> 2;
    int dst_base;

    for (plane = 0; plane < 4; plane++) {
        for (row = 0; row < height; row++) {
            dst_base = bufferofs + ylookup[y + row] + (x >> 2);
            for (col = 0; col < src_row_bytes; col++) {
                int src_idx = (plane * src_row_bytes * height) + (row * src_row_bytes) + col;
                screenbuffer[plane][dst_base + col] = source[src_idx];
            }
        }
    }
}

void VL_MaskedToScreen(byte far *source, int width, int height, int x, int y)
{
    // For now, same as MemToScreen (masking not implemented)
    VL_MemToScreen(source, width, height, x, y);
}

void VL_LatchToScreen(unsigned source, int width, int height, int x, int y)
{
    int plane, row, col;
    int src_row_bytes = width;
    int dst_base;

    for (plane = 0; plane < 4; plane++) {
        for (row = 0; row < height; row++) {
            dst_base = bufferofs + ylookup[y + row] + (x >> 2);
            for (col = 0; col < src_row_bytes; col++) {
                screenbuffer[plane][dst_base + col] = screenbuffer[plane][source + ylookup[row] + col];
            }
        }
    }
}

void VL_DrawTile8String(char *str, char far *tile8ptr, int printx, int printy)
{
    (void)str;
    (void)tile8ptr;
    (void)printx;
    (void)printy;
}

void VL_DrawLatch8String(char *str, unsigned tile8ptr, int printx, int printy)
{
    (void)str;
    (void)tile8ptr;
    (void)printx;
    (void)printy;
}

void VL_SizeTile8String(char *str, int *width, int *height)
{
    (void)str;
    *width = 0;
    *height = 8;
}

void VL_DrawPropString(char *str, unsigned tile8ptr, int printx, int printy)
{
    (void)str;
    (void)tile8ptr;
    (void)printx;
    (void)printy;
}

void VL_SizePropString(char *str, int *width, int *height, char far *font)
{
    (void)str;
    (void)font;
    *width = 0;
    *height = 0;
}

void VL_TestPaletteSet(void)
{
}

//
// Copy consecutive-plane source data to planar screen buffer
//
void VL_CopyPlanarToScreen(byte *source, long expanded)
{
    int plane_size = (int)(expanded / 4);
    int plane;
    for (plane = 0; plane < 4; plane++) {
        memcpy(screenbuffer[plane] + bufferofs, source + plane * plane_size, plane_size);
    }
}

//
// Present the current frame to SDL
//
static int update_count = 0;

void VH_UpdateScreen(void)
{
    int plane, i;
    int non_zero = 0;

    if (!sdl_texture) {
        fprintf(stderr, "VH_UpdateScreen: no texture!\n");
        return;
    }

    // Count non-zero pixels in screen buffer
    for (plane = 0; plane < 4 && non_zero < 10; plane++) {
        for (i = 0; i < 16000 && non_zero < 10; i++) {
            if (screenbuffer[plane][bufferofs + i] != 0)
                non_zero++;
        }
    }

    update_count++;

    PlanarToLinear();

    SDL_UpdateTexture(sdl_texture, NULL, linear_buffer, 320 * sizeof(Uint32));
    SDL_RenderClear(sdl_renderer);
    SDL_RenderTexture(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);

    SDL_PumpEvents();
}
