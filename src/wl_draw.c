// WL_DRAW.C - SDL3 software raycaster
//
// Port of the original DOS WL_DRAW.C + WL_DR_A.ASM. The original used
// VGA hardware planar tricks (bitmask register, compiled scalers, far-pointer
// segment+offset for postsource) — we don't. We just write straight into the
// emulated planar screenbuffer in id_vl.c.

#include "WL_DEF.H"

// The door pic block ends 8 entries before the sprite start in VSWAP.
#define DOORWALL	(PMSpriteStart-8)
#define ACTORSIZE	0x4000

// Fineangle constants from the original asm.
#define DEG90	(FINEANGLES/4)
#define DEG180	(FINEANGLES/2)
#define DEG270	(FINEANGLES*3/4)

unsigned screenloc[3] = {PAGE1START, PAGE2START, PAGE3START};
unsigned freelatch = 0;

long	lasttimecount = 0;
long	frameon = 0;

unsigned	wallheight[MAXVIEWWIDTH];

fixed	tileglobal	= TILEGLOBAL;
fixed	mindist		= MINDIST;

int		pixelangle[MAXVIEWWIDTH];
long	finetangent[FINEANGLES/4];
fixed	sintable[ANGLES+ANGLES/4],
		*costable = sintable + (ANGLES/4);

fixed	viewx, viewy;
int		viewangle;
fixed	viewsin, viewcos;

// Ray tracing scratch — exposed as globals because the Hit handlers read
// them. Direct port of the original DOS naming.
int			focaltx, focalty, viewtx, viewty;
int			midangle, angle;
unsigned	xpartial, ypartial;
unsigned	xpartialup, xpartialdown, ypartialup, ypartialdown;
unsigned	xinttile, yinttile;
unsigned	tilehit;
unsigned	pixx;

int		xtile, ytile;
int		xtilestep, ytilestep;
long	xintercept, yintercept;
long	xstep, ystep;

int		horizwall[MAXWALLTILES], vertwall[MAXWALLTILES];

byte		*postsource;
unsigned	postx;
unsigned	postwidth;

// We removed the lastside/lastintercept widen optimization (DOS-era CPU/VGA
// trick that bought multi-pixel column draws via the bit mask register). On a
// modern CPU it isn't worth the bookkeeping — every column does its own
// ScalePost call.

//==========================================================================
// Math helpers
//==========================================================================

fixed FixedByFrac (fixed a, fixed b)
{
	int sign = (b < 0);
	long long fa = (long long)a;
	long long fb = (long long)(b & 0x7fffffffl);
	long long result = (fa * fb) >> 16;
	if (sign)
		result = -result;
	return (fixed)result;
}

//==========================================================================
// Vertical column projection — the asm "CalcHeight" returned heightnumerator
// divided by (perpendicular distance >> 8).
//==========================================================================

int CalcHeight (void)
{
	fixed gxt, gyt, nx;
	long gx, gy;

	gx = xintercept - viewx;
	gxt = FixedByFrac(gx, viewcos);

	gy = yintercept - viewy;
	gyt = FixedByFrac(gy, viewsin);

	nx = gxt - gyt;
	if (nx < mindist)
		nx = mindist;

	long h = heightnumerator / (nx >> 8);
	if (h < 0) h = 0;
	if (h > 0xffff) h = 0xffff;
	return (int)h;
}

//==========================================================================
// ScalePost — draw one textured vertical strip of wall.
//
// Reads:
//   wallheight[postx] — column height in 8x precision; pixel height = h>>3
//   postsource        — points at first byte of the chosen 64-tall texture column
//   postx, postwidth  — viewport column and pixel width
//==========================================================================

void ScalePost (void)
{
	// wallheight is in 8x precision. The original asm picks a "compiled scaler"
	// at index wallheight/8; that scaler emits index*2 pixel rows. So pixel
	// height = wallheight/4. Aligning down to 4 matches that quantisation.
	int height = (wallheight[postx] & ~3) >> 2;
	if (height <= 0)
		return;

	int top = (viewheight - height) / 2;
	int bottom = top + height;
	int draw_top = top < 0 ? 0 : top;
	int draw_bot = bottom > viewheight ? viewheight : bottom;

	// fixed-point texture step: 64 texture rows over `height` pixels
	unsigned tex_step = (64u << 16) / (unsigned)height;
	unsigned tex_pos = (unsigned)(draw_top - top) * tex_step;

	unsigned w = postwidth ? postwidth : 1;
	int x0 = (int)postx;

	for (int y = draw_top; y < draw_bot; y++, tex_pos += tex_step) {
		byte color = postsource[tex_pos >> 16];
		for (unsigned i = 0; i < w; i++) {
			int x = x0 + (int)i;
			int plane = x & 3;
			int addr = bufferofs + ylookup[y] + (x >> 2);
			screenbuffer[plane][addr] = color;
		}
	}
}

void FarScalePost (void)
{
	ScalePost();
}

//==========================================================================
// Hit handlers — invoked from AsmRefresh when a ray strikes a tile. They
// finish setting up xintercept/yintercept/xtile/ytile for the texture lookup,
// pick the wall pic, and shoot the column to ScalePost.
//==========================================================================

void HitVertWall (void)
{
	int wallpic;
	unsigned texture;

	texture = (yintercept >> 4) & 0xfc0;
	if (xtilestep == -1) {
		texture = 0xfc0 - texture;
		xintercept += TILEGLOBAL;
	}
	wallheight[pixx] = CalcHeight();

	if (tilehit & 0x40) {
		// adjacent-door darkening: pick door-side pic
		int yt = yintercept >> TILESHIFT;
		if (tilemap[xtile - xtilestep][yt] & 0x80)
			wallpic = DOORWALL + 3;
		else
			wallpic = vertwall[tilehit & ~0x40];
	} else {
		wallpic = vertwall[tilehit];
	}

	byte *page = (byte *)PM_GetPage(wallpic);
	if (!page) return;
	postsource = page + texture;
	postx = pixx;
	postwidth = 1;
	ScalePost();
}

void HitHorizWall (void)
{
	int wallpic;
	unsigned texture;

	texture = (xintercept >> 4) & 0xfc0;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL;
	else
		texture = 0xfc0 - texture;
	wallheight[pixx] = CalcHeight();

	if (tilehit & 0x40) {
		int xt = xintercept >> TILESHIFT;
		if (tilemap[xt][ytile - ytilestep] & 0x80)
			wallpic = DOORWALL + 2;
		else
			wallpic = horizwall[tilehit & ~0x40];
	} else {
		wallpic = horizwall[tilehit];
	}

	byte *page = (byte *)PM_GetPage(wallpic);
	if (!page) return;
	postsource = page + texture;
	postx = pixx;
	postwidth = 1;
	ScalePost();
}

void HitHorizDoor (void)
{
	unsigned texture, doorpage;
	unsigned doornum = tilehit & 0x7f;

	texture = ((xintercept - doorposition[doornum]) >> 4) & 0xfc0;
	wallheight[pixx] = CalcHeight();

	switch (doorobjlist[doornum].lock) {
	case dr_normal:	doorpage = DOORWALL; break;
	case dr_lock1:
	case dr_lock2:
	case dr_lock3:
	case dr_lock4:	doorpage = DOORWALL + 6; break;
	case dr_elevator: doorpage = DOORWALL + 4; break;
	default: doorpage = DOORWALL; break;
	}

	byte *page = (byte *)PM_GetPage(doorpage);
	if (!page) return;
	postsource = page + texture;
	postx = pixx;
	postwidth = 1;
	ScalePost();
}

void HitVertDoor (void)
{
	unsigned texture, doorpage;
	unsigned doornum = tilehit & 0x7f;

	texture = ((yintercept - doorposition[doornum]) >> 4) & 0xfc0;
	wallheight[pixx] = CalcHeight();

	switch (doorobjlist[doornum].lock) {
	case dr_normal:	doorpage = DOORWALL; break;
	case dr_lock1:
	case dr_lock2:
	case dr_lock3:
	case dr_lock4:	doorpage = DOORWALL + 6; break;
	case dr_elevator: doorpage = DOORWALL + 4; break;
	default: doorpage = DOORWALL; break;
	}

	byte *page = (byte *)PM_GetPage(doorpage + 1);
	if (!page) return;
	postsource = page + texture;
	postx = pixx;
	postwidth = 1;
	ScalePost();
}

void HitHorizPWall (void)
{
	int wallpic;
	unsigned texture, offset;

	texture = (xintercept >> 4) & 0xfc0;
	offset = pwallpos << 10;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL - offset;
	else {
		texture = 0xfc0 - texture;
		yintercept += offset;
	}

	wallheight[pixx] = CalcHeight();
	wallpic = horizwall[tilehit & 63];

	byte *page = (byte *)PM_GetPage(wallpic);
	if (!page) return;
	postsource = page + texture;
	postx = pixx;
	postwidth = 1;
	ScalePost();
}

void HitVertPWall (void)
{
	int wallpic;
	unsigned texture, offset;

	texture = (yintercept >> 4) & 0xfc0;
	offset = pwallpos << 10;
	if (xtilestep == -1) {
		xintercept += TILEGLOBAL - offset;
		texture = 0xfc0 - texture;
	} else {
		xintercept += offset;
	}

	wallheight[pixx] = CalcHeight();
	wallpic = vertwall[tilehit & 63];

	byte *page = (byte *)PM_GetPage(wallpic);
	if (!page) return;
	postsource = page + texture;
	postx = pixx;
	postwidth = 1;
	ScalePost();
}

//==========================================================================
// AsmRefresh — tile-DDA ray tracer. Direct C port of WL_DR_A.ASM
// (without the asm-only postwidth widening optimization).
//==========================================================================

// 32-bit fixed-point multiply by a 16-bit fraction. The original asm
// helpers (xpartialbyystep / ypartialbyxstep) compute (signed long * unsigned word).
static long mul_step_by_partial (long step, unsigned partial)
{
	int neg = step < 0;
	unsigned long mag = neg ? (unsigned long)(-step) : (unsigned long)step;
	// magnitude is at most 32 bits; partial fits in 16. Product fits in 64.
	unsigned long long product = (unsigned long long)mag * (unsigned long long)partial;
	long result = (long)(product >> 16);
	return neg ? -result : result;
}

void AsmRefresh (void)
{
	for (pixx = 0; pixx < (unsigned)viewwidth; pixx++) {
		int angl = midangle + pixelangle[pixx];
		if (angl < 0) angl += FINEANGLES;
		if (angl >= FINEANGLES) angl -= FINEANGLES;

		// Quadrant setup: pick xstep/ystep signs and starting partials.
		if (angl < DEG90) {
			xtilestep = 1; ytilestep = -1;
			xstep =  finetangent[DEG90 - 1 - angl];
			ystep = -finetangent[angl];
			xpartial = xpartialup;
			ypartial = ypartialdown;
		} else if (angl < DEG180) {
			xtilestep = -1; ytilestep = -1;
			xstep = -finetangent[angl - DEG90];
			ystep = -finetangent[DEG180 - 1 - angl];
			xpartial = xpartialdown;
			ypartial = ypartialdown;
		} else if (angl < DEG270) {
			xtilestep = -1; ytilestep = 1;
			xstep = -finetangent[DEG270 - 1 - angl];
			ystep =  finetangent[angl - DEG180];
			xpartial = xpartialdown;
			ypartial = ypartialup;
		} else {
			xtilestep = 1; ytilestep = 1;
			xstep =  finetangent[angl - DEG270];
			ystep =  finetangent[FINEANGLES - 1 - angl];
			xpartial = xpartialup;
			ypartial = ypartialup;
		}

		// Initial intercepts: from the focal point step xpartial worth of x
		// (or ypartial worth of y) and project the ray to the first grid line.
		yintercept = viewy + mul_step_by_partial(ystep, xpartial);
		xtile = focaltx + xtilestep;

		xintercept = viewx + mul_step_by_partial(xstep, ypartial);
		ytile = focalty + ytilestep;

		unsigned yint_tile = (unsigned)(yintercept >> TILESHIFT);
		unsigned xint_tile = (unsigned)(xintercept >> TILESHIFT);

		byte *tilebase = &tilemap[0][0];
		byte *visbase  = &spotvis[0][0];

		// Cap iterations to avoid runaway tracing on a malformed map.
		int safety = 256;

		while (safety--) {
			// Vertical-wall check: ray crossing the next X-tile boundary first?
			int crossed_y;
			if (ytilestep == -1)
				crossed_y = ((int)yint_tile <= ytile);
			else
				crossed_y = ((int)yint_tile >= ytile);

			if (!crossed_y) {
				// vertentry
				unsigned xspot = (unsigned)xtile * MAPSIZE + yint_tile;
				if (xspot >= MAPSIZE*MAPSIZE) break;
				tilehit = tilebase[xspot];
				if (tilehit) {
					if (!(tilehit & 0x80)) {
						// solid wall
						xintercept = (long)xtile << TILESHIFT;
						yintercept = ((long)yint_tile << TILESHIFT)
									| (yintercept & 0xffff);
						ytile = (int)yint_tile;
						HitVertWall();
						break;
					}
					// door / pwall
					if (tilehit & 0x40) {
						long ds = (long)(((long long)ystep * pwallpos) >> 6);
						long ny = yintercept + ds;
						if ((unsigned)(ny >> TILESHIFT) != yint_tile) {
							// fell out of tile — continue tracing
							goto passvert;
						}
						yintercept = ny;
						xintercept = (long)xtile << TILESHIFT;
						HitVertPWall();
						break;
					} else {
						long ds = ystep >> 1;
						long ny = yintercept + ds;
						if ((unsigned)(ny >> TILESHIFT) != yint_tile)
							goto passvert;
						unsigned dnum = tilehit & 0x7f;
						if ((unsigned)(ny & 0xffff) < doorposition[dnum])
							goto passvert;
						yintercept = (ny & 0xffff)
									| ((long)yint_tile << TILESHIFT);
						xintercept = ((long)xtile << TILESHIFT) | 0x8000;
						HitVertDoor();
						break;
					}
				}
			passvert:
				{
					unsigned vspot = (unsigned)xtile * MAPSIZE + yint_tile;
					if (vspot < MAPSIZE*MAPSIZE)
						visbase[vspot] = 1;
					xtile += xtilestep;
					yintercept += ystep;
					yint_tile = (unsigned)(yintercept >> TILESHIFT);
				}
				continue;
			}

			// Horizontal-wall check
			int crossed_x;
			if (xtilestep == -1)
				crossed_x = ((int)xint_tile <= xtile);
			else
				crossed_x = ((int)xint_tile >= xtile);

			if (crossed_x) {
				// neither boundary closer — degenerate, bail to avoid spin
				break;
			}

			unsigned yspot = xint_tile * MAPSIZE + (unsigned)ytile;
			if (yspot >= MAPSIZE*MAPSIZE) break;
			tilehit = tilebase[yspot];
			if (tilehit) {
				if (!(tilehit & 0x80)) {
					xintercept = ((long)xint_tile << TILESHIFT)
								| (xintercept & 0xffff);
					xtile = (int)xint_tile;
					yintercept = (long)ytile << TILESHIFT;
					HitHorizWall();
					break;
				}
				if (tilehit & 0x40) {
					long ds = (long)(((long long)xstep * pwallpos) >> 6);
					long nx = xintercept + ds;
					if ((unsigned)(nx >> TILESHIFT) != xint_tile)
						goto passhoriz;
					xintercept = nx;
					yintercept = (long)ytile << TILESHIFT;
					HitHorizPWall();
					break;
				} else {
					long ds = xstep >> 1;
					long nx = xintercept + ds;
					if ((unsigned)(nx >> TILESHIFT) != xint_tile)
						goto passhoriz;
					unsigned dnum = tilehit & 0x7f;
					if ((unsigned)(nx & 0xffff) < doorposition[dnum])
						goto passhoriz;
					xintercept = (nx & 0xffff)
								| ((long)xint_tile << TILESHIFT);
					yintercept = ((long)ytile << TILESHIFT) | 0x8000;
					HitHorizDoor();
					break;
				}
			}
		passhoriz:
			{
				unsigned hspot = xint_tile * MAPSIZE + (unsigned)ytile;
				if (hspot < MAPSIZE*MAPSIZE)
					visbase[hspot] = 1;
				ytile += ytilestep;
				xintercept += xstep;
				xint_tile = (unsigned)(xintercept >> TILESHIFT);
			}
		}
	}
}

//==========================================================================
// Ceiling/floor fill — episode 1 uses 0x1d/0x19 by default.
//==========================================================================

static const unsigned vgaCeiling[] = {
	0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0xbfbf,
	0x4e4e, 0x4e4e, 0x4e4e, 0x1d1d, 0x8d8d, 0x4e4e, 0x1d1d, 0x2d2d, 0x1d1d, 0x8d8d,
	0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0x2d2d, 0xdddd, 0x1d1d, 0x1d1d, 0x9898,
	0x1d1d, 0x9d9d, 0x2d2d, 0xdddd, 0xdddd, 0x9d9d, 0x2d2d, 0x4d4d, 0x1d1d, 0xdddd,
	0x7d7d, 0x1d1d, 0x2d2d, 0x2d2d, 0xdddd, 0xd7d7, 0x1d1d, 0x1d1d, 0x1d1d, 0x2d2d,
	0x1d1d, 0x1d1d, 0x1d1d, 0x1d1d, 0xdddd, 0xdddd, 0x7d7d, 0xdddd, 0xdddd, 0xdddd
};

void VGAClearScreen (void)
{
	int idx = gamestate.episode * 10 + gamestate.mapon;
	if (idx < 0) idx = 0;
	if (idx >= (int)(sizeof(vgaCeiling) / sizeof(vgaCeiling[0])))
		idx = 0;

	byte ceil = (byte)(vgaCeiling[idx] & 0xff);
	byte floor = 0x19;

	int half = viewheight / 2;
	int vw_bytes = viewwidth / 4;  // plane bytes per row

	for (int y = 0; y < viewheight; y++) {
		byte color = (y < half) ? ceil : floor;
		int row_base = bufferofs + ylookup[y];
		for (int plane = 0; plane < 4; plane++)
			memset(screenbuffer[plane] + row_base, color, vw_bytes);
	}
}

void ClearScreen (void)
{
	VGAClearScreen();
}

//==========================================================================
// WallRefresh / ThreeDRefresh
//==========================================================================

void WallRefresh (void)
{
	viewangle = player->angle;
	midangle = viewangle * (FINEANGLES / ANGLES);
	viewsin = sintable[viewangle];
	viewcos = costable[viewangle];
	viewx = player->x - FixedByFrac(focallength, viewcos);
	viewy = player->y + FixedByFrac(focallength, viewsin);

	focaltx = viewx >> TILESHIFT;
	focalty = viewy >> TILESHIFT;

	viewtx = player->x >> TILESHIFT;
	viewty = player->y >> TILESHIFT;

	xpartialdown = viewx & (TILEGLOBAL - 1);
	xpartialup = TILEGLOBAL - xpartialdown;
	ypartialdown = viewy & (TILEGLOBAL - 1);
	ypartialup = TILEGLOBAL - ypartialdown;

	AsmRefresh();
}

void CalcTics (void)
{
	tics = 1;	// 70 Hz target — adaptive timing would compute from TimeCount
}

void FixOfs (void)
{
}

void CheckIs386 (void)
{
}

void jabhack2 (void)
{
}

//==========================================================================
// Sprite scaling
//
// Sprite layout (read straight out of VSWAP byte-for-byte):
//   int16  leftpix, rightpix
//   uint16 dataofs[rightpix - leftpix + 1]   // per-column command-list offset
//   ...post data...
//
// A column's command list is a sequence of 3-uint16 records:
//   [end_row*2]    0 sentinel terminates the list
//   [data_bias]    add to this to get pixel-byte for virtual row r
//                  (i.e. row r in [start, end) is at &page[data_bias + r])
//   [start_row*2]
//==========================================================================

static int sprite_clamp_height (unsigned height)
{
	int h = (int)height >> 2;
	if (h < 0) h = 0;
	if (h > MAXSCALEHEIGHT*2) h = MAXSCALEHEIGHT*2;
	return h;
}

void ScaleShape (int xcenter, int shapenum, unsigned height)
{
	byte *page = (byte *)PM_GetSpritePage(shapenum);
	if (!page) return;

	int sh = sprite_clamp_height(height);
	if (sh <= 0) return;

	int leftpix  = *(int16_t *)(page + 0);
	int rightpix = *(int16_t *)(page + 2);
	uint16_t *dataofs = (uint16_t *)(page + 4);

	int left_x = xcenter - sh / 2;
	int top_y  = (viewheight - sh) / 2;

	for (int src_col = leftpix; src_col <= rightpix; src_col++) {
		int dx0 = left_x + (src_col * sh) / 64;
		int dx1 = left_x + ((src_col + 1) * sh) / 64;
		if (dx1 <= 0 || dx0 >= viewwidth) continue;
		if (dx1 == dx0) dx1 = dx0 + 1;	// guarantee at least one pixel per source col

		uint16_t *cmd = (uint16_t *)(page + dataofs[src_col - leftpix]);

		while (cmd[0]) {
			int end_row   = cmd[0] >> 1;
			uint16_t bias = cmd[1];
			int start_row = cmd[2] >> 1;
			cmd += 3;

			int dy0 = top_y + (start_row * sh) / 64;
			int dy1 = top_y + (end_row   * sh) / 64;

			for (int dy = dy0; dy < dy1; dy++) {
				if (dy < 0 || dy >= viewheight) continue;
				int src_row = ((dy - top_y) * 64) / sh;
				if (src_row < start_row) src_row = start_row;
				else if (src_row >= end_row) src_row = end_row - 1;
				byte color = page[bias + src_row];

				int xa = dx0 < 0 ? 0 : dx0;
				int xb = dx1 > viewwidth ? viewwidth : dx1;
				for (int dx = xa; dx < xb; dx++) {
					if (wallheight[dx] >= height) continue;	// behind a closer wall
					int plane = dx & 3;
					int addr = bufferofs + ylookup[dy] + (dx >> 2);
					screenbuffer[plane][addr] = color;
				}
			}
		}
	}
}

void SimpleScaleShape (int xcenter, int shapenum, unsigned height)
{
	byte *page = (byte *)PM_GetSpritePage(shapenum);
	if (!page) return;

	int sh = (int)height;
	if (sh <= 0) return;

	int leftpix  = *(int16_t *)(page + 0);
	int rightpix = *(int16_t *)(page + 2);
	uint16_t *dataofs = (uint16_t *)(page + 4);

	int left_x = xcenter - sh / 2;
	int top_y  = viewheight - sh;	// weapon sits flush against the viewport bottom

	for (int src_col = leftpix; src_col <= rightpix; src_col++) {
		int dx0 = left_x + (src_col * sh) / 64;
		int dx1 = left_x + ((src_col + 1) * sh) / 64;
		if (dx1 <= 0 || dx0 >= viewwidth) continue;
		if (dx1 == dx0) dx1 = dx0 + 1;

		uint16_t *cmd = (uint16_t *)(page + dataofs[src_col - leftpix]);

		while (cmd[0]) {
			int end_row   = cmd[0] >> 1;
			uint16_t bias = cmd[1];
			int start_row = cmd[2] >> 1;
			cmd += 3;

			int dy0 = top_y + (start_row * sh) / 64;
			int dy1 = top_y + (end_row   * sh) / 64;

			for (int dy = dy0; dy < dy1; dy++) {
				if (dy < 0 || dy >= viewheight) continue;
				int src_row = ((dy - top_y) * 64) / sh;
				if (src_row < start_row) src_row = start_row;
				else if (src_row >= end_row) src_row = end_row - 1;
				byte color = page[bias + src_row];

				int xa = dx0 < 0 ? 0 : dx0;
				int xb = dx1 > viewwidth ? viewwidth : dx1;
				for (int dx = xa; dx < xb; dx++) {
					int plane = dx & 3;
					int addr = bufferofs + ylookup[dy] + (dx >> 2);
					screenbuffer[plane][addr] = color;
				}
			}
		}
	}
}

//==========================================================================
// Actor / static-object projection + culling
//==========================================================================

void TransformActor (objtype *ob)
{
	fixed gx = ob->x - viewx;
	fixed gy = ob->y - viewy;

	fixed gxt = FixedByFrac(gx, viewcos);
	fixed gyt = FixedByFrac(gy, viewsin);
	fixed nx = gxt - gyt - ACTORSIZE;	// nudge midpoint forward so corners don't poke through walls

	gxt = FixedByFrac(gx, viewsin);
	gyt = FixedByFrac(gy, viewcos);
	fixed ny = gyt + gxt;

	ob->transx = nx;
	ob->transy = ny;

	if (nx < mindist) {
		ob->viewheight = 0;
		return;
	}

	ob->viewx = centerx + (int)(((long long)ny * scale) / nx);

	long h = heightnumerator / (nx >> 8);
	if (h < 0) h = 0;
	if (h > 0xffff) h = 0xffff;
	ob->viewheight = (unsigned)h;
}

boolean TransformTile (int tx, int ty, int *dispx, int *dispheight)
{
	fixed gx = ((long)tx << TILESHIFT) + 0x8000 - viewx;
	fixed gy = ((long)ty << TILESHIFT) + 0x8000 - viewy;

	fixed gxt = FixedByFrac(gx, viewcos);
	fixed gyt = FixedByFrac(gy, viewsin);
	fixed nx = gxt - gyt - 0x2000;

	gxt = FixedByFrac(gx, viewsin);
	gyt = FixedByFrac(gy, viewcos);
	fixed ny = gyt + gxt;

	if (nx < mindist) {
		*dispheight = 0;
		return false;
	}

	*dispx = centerx + (int)(((long long)ny * scale) / nx);

	long h = heightnumerator / (nx >> 8);
	if (h < 0) h = 0;
	if (h > 0xffff) h = 0xffff;
	*dispheight = (int)h;

	return (nx < TILEGLOBAL && ny > -TILEGLOBAL/2 && ny < TILEGLOBAL/2);
}

int CalcRotate (objtype *ob)
{
	int va = player->angle + (centerx - ob->viewx) / 8;
	int angl;

	if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
		angl = (va - 180) - ob->angle;
	else
		angl = (va - 180) - dirangle[ob->dir];

	angl += ANGLES / 16;
	while (angl >= ANGLES) angl -= ANGLES;
	while (angl < 0)       angl += ANGLES;

	// Original supported rotate==2 (multi-value enum, 2-rotation pain frames).
	// boolean here is C99 bool so that case is unreachable — pain frames will
	// just use the regular 8-rotation lookup. Cosmetic only.
	return angl / (ANGLES / 8);
}

//==========================================================================
// DrawScaleds — gather visible sprites, sort back-to-front, draw.
//==========================================================================

#define MAXVISIBLE	64

typedef struct {
	int			viewx;
	int			viewheight;
	int			shapenum;
} visobj_t;

static visobj_t vislist[MAXVISIBLE];

void DrawScaleds (void)
{
	visobj_t	*visptr = vislist;

	// Static objects
	for (statobj_t *st = statobjlist; st != laststatobj; st++) {
		if ((visptr->shapenum = st->shapenum) == -1)
			continue;
		if (!*st->visspot)
			continue;

		int vx, vh;
		if (TransformTile(st->tilex, st->tiley, &vx, &vh)
			&& (st->flags & FL_BONUS)) {
			GetBonus(st);
			continue;
		}
		if (!vh) continue;

		visptr->viewx = vx;
		visptr->viewheight = vh;
		if (visptr < &vislist[MAXVISIBLE - 1])
			visptr++;
	}

	// Actors (any one of the 9 surrounding tiles being visible counts)
	for (objtype *o = player->next; o; o = o->next) {
		if (!(visptr->shapenum = o->state->shapenum))
			continue;

		int spotloc = (int)o->tilex * MAPSIZE + (int)o->tiley;
		// Don't walk off the spotvis/tilemap edges (asm didn't check; we do).
		if (spotloc < MAPSIZE + 1 || spotloc >= MAPSIZE*MAPSIZE - MAPSIZE - 1)
			continue;

		byte *vs = &spotvis[0][0] + spotloc;
		byte *ts = &tilemap[0][0] + spotloc;

		int visible =
			vs[0] ||
			(vs[-1]  && !ts[-1]) ||
			(vs[+1]  && !ts[+1]) ||
			(vs[-65] && !ts[-65]) ||
			(vs[-64] && !ts[-64]) ||
			(vs[-63] && !ts[-63]) ||
			(vs[+63] && !ts[+63]) ||
			(vs[+64] && !ts[+64]) ||
			(vs[+65] && !ts[+65]);

		if (!visible) {
			o->flags &= ~FL_VISABLE;
			continue;
		}

		o->active = ac_yes;
		TransformActor(o);
		if (!o->viewheight) continue;

		visptr->viewx = o->viewx;
		visptr->viewheight = o->viewheight;
		if (visptr->shapenum == -1)
			visptr->shapenum = o->temp1;
		if (o->state->rotate)
			visptr->shapenum += CalcRotate(o);

		if (visptr < &vislist[MAXVISIBLE - 1])
			visptr++;
		o->flags |= FL_VISABLE;
	}

	int n = (int)(visptr - vislist);
	for (int i = 0; i < n; i++) {
		int least = 32000;
		visobj_t *far_ = NULL;
		for (visobj_t *vs = vislist; vs < visptr; vs++) {
			if (vs->viewheight < least) {
				least = vs->viewheight;
				far_ = vs;
			}
		}
		if (!far_) break;
		ScaleShape(far_->viewx, far_->shapenum, far_->viewheight);
		far_->viewheight = 32000;
	}
}

//==========================================================================
// Player weapon — held weapon sprite, drawn after world + scaled actors.
//==========================================================================

static int weaponscale[NUMWEAPONS] = {
	SPR_KNIFEREADY, SPR_PISTOLREADY, SPR_MACHINEGUNREADY, SPR_CHAINREADY
};

void DrawPlayerWeapon (void)
{
#ifndef SPEAR
	if (gamestate.victoryflag) {
		if (player->state == &s_deathcam && (TimeCount & 32))
			SimpleScaleShape(viewwidth/2, SPR_DEATHCAM, viewheight + 1);
		return;
	}
#endif

	if (gamestate.weapon != -1) {
		int shapenum = weaponscale[gamestate.weapon] + gamestate.weaponframe;
		SimpleScaleShape(viewwidth/2, shapenum, viewheight + 1);
	}

	if (demorecord || demoplayback)
		SimpleScaleShape(viewwidth/2, SPR_DEMO, viewheight + 1);
}

void ThreeDRefresh (void)
{
	// Single-buffer port: always draw into page 1.  bufferofs gets the
	// viewport offset added for the duration of world drawing, then restored
	// so subsequent HUD updates (which write at bufferofs + status-bar offset)
	// land in the right place.
	memset(spotvis, 0, sizeof(spotvis));

	bufferofs = PAGE1START + screenofs;

	VGAClearScreen();
	WallRefresh();
	DrawScaleds();
	DrawPlayerWeapon();

	bufferofs = PAGE1START;
	displayofs = PAGE1START;

	// Original DOS triggered the page flip via CRTC — that's what made the
	// new frame visible. We don't have a CRTC, so we explicitly tell the SDL3
	// backend to present.  Without this, gameplay shows a frozen frame even
	// though the player object is moving every tic.
	VW_UpdateScreen();

	frameon++;
}
