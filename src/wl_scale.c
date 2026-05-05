// WL_SCALE.C - SDL3 stub implementation

#include "WL_DEF.H"

t_compscale *scaledirectory[MAXSCALEHEIGHT+1];
long			fullscalefarcall[MAXSCALEHEIGHT+1];

int			maxscale,maxscaleshl2;

boolean		insetupscaling;

t_compscale *work;

int			stepbytwo;

void ScaleLine (void)
{
}

void ScaleShape (int xcenter, int shapenum, unsigned height)
{
}

void SimpleScaleShape (int xcenter, int shapenum, unsigned height)
{
}

void SetupScaling (int maxscaleheight)
{
	insetupscaling = true;
	maxscale = maxscaleheight/2 - 1;
	maxscaleshl2 = maxscale<<2;
}

void far BadScale (void)
{
	Quit ("BadScale called!");
}
