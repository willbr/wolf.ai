// WL_DRAW.C - SDL3 stub implementation

#include "WL_DEF.H"

unsigned screenloc[3]= {0,0,0};
unsigned freelatch = 0;

long 	lasttimecount = 0;
long 	frameon = 0;

unsigned	wallheight[MAXVIEWWIDTH];

fixed	tileglobal	= TILEGLOBAL;
fixed	mindist		= MINDIST;

int			pixelangle[MAXVIEWWIDTH];
long		finetangent[FINEANGLES/4];
fixed 		sintable[ANGLES+ANGLES/4],
			*costable = sintable+(ANGLES/4);

fixed	viewx,viewy;
int		viewangle;
fixed	viewsin,viewcos;

int		lastside;
long	lastintercept;
int		lasttilehit;

int			focaltx,focalty,viewtx,viewty;
int			midangle,angle;
unsigned	xpartial,ypartial;
unsigned	xpartialup,xpartialdown,ypartialup,ypartialdown;
unsigned	xinttile,yinttile;
unsigned	tilehit;
unsigned	pixx;
int		xtile,ytile;
int		xtilestep,ytilestep;
long	xintercept,yintercept;
long	xstep,ystep;

int		horizwall[MAXWALLTILES],vertwall[MAXWALLTILES];

long		postsource;
unsigned	postx;
unsigned	postwidth;

fixed	FixedByFrac (fixed a, fixed b)
{
	return 0;
}

void AsmRefresh (void)
{
}

void TransformActor (objtype *ob)
{
}

boolean TransformTile (int tx, int ty, int *dispx, int *dispheight)
{
	*dispx = 0;
	*dispheight = 0;
	return false;
}

void FarScalePost (void)
{
}

void HitVertWall (void)
{
}

void HitHorizWall (void)
{
}

void HitHorizDoor (void)
{
}

void HitVertDoor (void)
{
}

void HitHorizPWall (void)
{
}

void HitVertPWall (void)
{
}

void ClearScreen (void)
{
}

void VGAClearScreen (void)
{
}

void DrawScaleds (void)
{
}

void DrawPlayerWeapon (void)
{
}

void CalcTics (void)
{
	tics = 1; // Stub: assume 70Hz frame rate
}

void WallRefresh (void)
{
}

void FixOfs (void)
{
}

void ThreeDRefresh (void)
{
}

void CheckIs386(void)
{
}

void jabhack2(void)
{
}
