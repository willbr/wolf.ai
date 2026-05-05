// ID_IN.C - SDL3 stub implementation

#include "ID_HEADS.H"

boolean		Keyboard[NumCodes];
boolean		MousePresent = false;
boolean		JoysPresent[MaxJoys] = {false, false};
boolean		Paused = false;
char		LastASCII = 0;
ScanCode	LastScan = sc_None;
KeyboardDef	KbdDefs;
JoystickDef	JoyDefs[MaxJoys];
ControlType	Controls[MaxPlayers];

Demo		DemoMode = demo_Off;
byte _seg	*DemoBuffer = NULL;
word		DemoOffset = 0;
word		DemoSize = 0;

void IN_Startup(void)
{
	int i;
	for (i = 0; i < NumCodes; i++)
		Keyboard[i] = false;
}

void IN_Shutdown(void)
{
}

void IN_Default(boolean gotit, ControlType in)
{
	Controls[0] = ctrl_Keyboard;
}

void IN_SetKeyHook(void (*hook)())
{
}

void IN_ClearKeysDown(void)
{
	int i;
	for (i = 0; i < NumCodes; i++)
		Keyboard[i] = false;
	LastScan = sc_None;
	LastASCII = 0;
}

void IN_ReadCursor(CursorInfo *ci)
{
	ci->x = 0;
	ci->y = 0;
	ci->button0 = false;
	ci->button1 = false;
	ci->button2 = false;
	ci->button3 = false;
	ci->xaxis = motion_None;
	ci->yaxis = motion_None;
	ci->dir = dir_None;
}

void IN_ReadControl(int player, ControlInfo *ci)
{
	IN_ReadCursor((CursorInfo *)ci);
}

void IN_SetControlType(int player, ControlType type)
{
	Controls[player] = type;
}

void IN_GetJoyAbs(word joy, word *xp, word *yp)
{
	*xp = 0;
	*yp = 0;
}

void IN_SetupJoy(word joy, word minx, word maxx, word miny, word maxy)
{
}

void IN_StopDemo(void)
{
	DemoMode = demo_Off;
}

void IN_FreeDemoBuffer(void)
{
	if (DemoBuffer)
	{
		MM_FreePtr((memptr *)&DemoBuffer);
		DemoBuffer = NULL;
	}
}

void IN_Ack(void)
{
	IN_ClearKeysDown();
}

void IN_AckBack(void)
{
	IN_ClearKeysDown();
}

boolean IN_UserInput(longword delay)
{
	return false;
}

char IN_WaitForASCII(void)
{
	return 0;
}

ScanCode IN_WaitForKey(void)
{
	return sc_None;
}

word IN_GetJoyButtonsDB(word joy)
{
	return 0;
}

byte *IN_GetScanName(ScanCode sc)
{
	return (byte *)"?";
}

byte IN_MouseButtons(void)
{
	return 0;
}

byte IN_JoyButtons(void)
{
	return 0;
}

void INL_GetJoyDelta(word joy, int *dx, int *dy)
{
	*dx = 0;
	*dy = 0;
}

void IN_StartAck(void)
{
}

boolean IN_CheckAck(void)
{
	return false;
}
