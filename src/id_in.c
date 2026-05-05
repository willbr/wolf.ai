// ID_IN.C - SDL3 Input Manager Implementation

#include "ID_HEADS.H"
#include <SDL3/SDL.h>

boolean		Keyboard[NumCodes];
boolean		MousePresent = false;
boolean		JoysPresent[MaxJoys] = {false, false};
boolean		Paused = false;
char		LastASCII = 0;
ScanCode	LastScan = sc_None;
KeyboardDef	KbdDefs = {0x1d,0x38,0x47,0x48,0x49,0x4b,0x4d,0x4f,0x50,0x51};
JoystickDef	JoyDefs[MaxJoys];
ControlType	Controls[MaxPlayers];

Demo		DemoMode = demo_Off;
byte _seg	*DemoBuffer = NULL;
word		DemoOffset = 0;
word		DemoSize = 0;

static boolean	btnstate[8];
static Uint64	last_pump_ms = 0;
static Uint64	accumulated_ms = 0;

//
// SDL scancode -> DOS scan code mapping
//
static ScanCode SDLToDOS(SDL_Scancode sc)
{
    switch (sc)
    {
        case SDL_SCANCODE_A: return sc_A;
        case SDL_SCANCODE_B: return sc_B;
        case SDL_SCANCODE_C: return sc_C;
        case SDL_SCANCODE_D: return sc_D;
        case SDL_SCANCODE_E: return sc_E;
        case SDL_SCANCODE_F: return sc_F;
        case SDL_SCANCODE_G: return sc_G;
        case SDL_SCANCODE_H: return sc_H;
        case SDL_SCANCODE_I: return sc_I;
        case SDL_SCANCODE_J: return sc_J;
        case SDL_SCANCODE_K: return sc_K;
        case SDL_SCANCODE_L: return sc_L;
        case SDL_SCANCODE_M: return sc_M;
        case SDL_SCANCODE_N: return sc_N;
        case SDL_SCANCODE_O: return sc_O;
        case SDL_SCANCODE_P: return sc_P;
        case SDL_SCANCODE_Q: return sc_Q;
        case SDL_SCANCODE_R: return sc_R;
        case SDL_SCANCODE_S: return sc_S;
        case SDL_SCANCODE_T: return sc_T;
        case SDL_SCANCODE_U: return sc_U;
        case SDL_SCANCODE_V: return sc_V;
        case SDL_SCANCODE_W: return sc_W;
        case SDL_SCANCODE_X: return sc_X;
        case SDL_SCANCODE_Y: return sc_Y;
        case SDL_SCANCODE_Z: return sc_Z;
        case SDL_SCANCODE_1: return sc_1;
        case SDL_SCANCODE_2: return sc_2;
        case SDL_SCANCODE_3: return sc_3;
        case SDL_SCANCODE_4: return sc_4;
        case SDL_SCANCODE_5: return sc_5;
        case SDL_SCANCODE_6: return sc_6;
        case SDL_SCANCODE_7: return sc_7;
        case SDL_SCANCODE_8: return sc_8;
        case SDL_SCANCODE_9: return sc_9;
        case SDL_SCANCODE_0: return sc_0;
        case SDL_SCANCODE_RETURN: return sc_Return;
        case SDL_SCANCODE_ESCAPE: return sc_Escape;
        case SDL_SCANCODE_SPACE: return sc_Space;
        case SDL_SCANCODE_BACKSPACE: return sc_BackSpace;
        case SDL_SCANCODE_TAB: return sc_Tab;
        case SDL_SCANCODE_UP: return sc_UpArrow;
        case SDL_SCANCODE_DOWN: return sc_DownArrow;
        case SDL_SCANCODE_LEFT: return sc_LeftArrow;
        case SDL_SCANCODE_RIGHT: return sc_RightArrow;
        case SDL_SCANCODE_INSERT: return sc_Insert;
        case SDL_SCANCODE_DELETE: return sc_Delete;
        case SDL_SCANCODE_HOME: return sc_Home;
        case SDL_SCANCODE_END: return sc_End;
        case SDL_SCANCODE_PAGEUP: return sc_PgUp;
        case SDL_SCANCODE_PAGEDOWN: return sc_PgDn;
        case SDL_SCANCODE_F1: return sc_F1;
        case SDL_SCANCODE_F2: return sc_F2;
        case SDL_SCANCODE_F3: return sc_F3;
        case SDL_SCANCODE_F4: return sc_F4;
        case SDL_SCANCODE_F5: return sc_F5;
        case SDL_SCANCODE_F6: return sc_F6;
        case SDL_SCANCODE_F7: return sc_F7;
        case SDL_SCANCODE_F8: return sc_F8;
        case SDL_SCANCODE_F9: return sc_F9;
        case SDL_SCANCODE_F10: return sc_F10;
        case SDL_SCANCODE_F11: return sc_F11;
        case SDL_SCANCODE_F12: return sc_F12;
        case SDL_SCANCODE_LSHIFT: return sc_LShift;
        case SDL_SCANCODE_RSHIFT: return sc_RShift;
        case SDL_SCANCODE_LCTRL: return sc_Control;
        case SDL_SCANCODE_RCTRL: return sc_Control;
        case SDL_SCANCODE_LALT: return sc_Alt;
        case SDL_SCANCODE_RALT: return sc_Alt;
        case SDL_SCANCODE_GRAVE: return 0x29; // backtick/tilde
        case SDL_SCANCODE_MINUS: return 0x0c;
        case SDL_SCANCODE_EQUALS: return 0x0d;
        case SDL_SCANCODE_LEFTBRACKET: return 0x1a;
        case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
        case SDL_SCANCODE_BACKSLASH: return 0x2b;
        case SDL_SCANCODE_SEMICOLON: return 0x27;
        case SDL_SCANCODE_APOSTROPHE: return 0x28;
        case SDL_SCANCODE_COMMA: return 0x33;
        case SDL_SCANCODE_PERIOD: return 0x34;
        case SDL_SCANCODE_SLASH: return 0x35;
        default: return sc_None;
    }
}

//
// Update TimeCount based on elapsed real time (target 70Hz)
//
static void UpdateTimeCount(void)
{
    Uint64 now = SDL_GetTicks();
    Uint64 elapsed = now - last_pump_ms;
    
    // Throttle to prevent burning CPU in tight loops
    if (elapsed < 5) {
        SDL_Delay((Uint32)(5 - elapsed));
        now = SDL_GetTicks();
        elapsed = now - last_pump_ms;
    }
    last_pump_ms = now;
    
    accumulated_ms += elapsed;
    while (accumulated_ms >= 14) {
        TimeCount++;
        accumulated_ms -= 14;
    }
}

//
// Poll SDL events and update keyboard state
//
static void IN_PumpEvents(void)
{
    SDL_Event event;
    
    UpdateTimeCount();
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_KEY_DOWN: {
                ScanCode sc = SDLToDOS(event.key.scancode);
                if (sc != sc_None) {
                    Keyboard[sc] = true;
                    LastScan = sc;
                }
                
                // Set LastASCII from key symbol
                SDL_Keycode sym = event.key.key;
                if (sym >= 32 && sym < 127) {
                    LastASCII = (char)sym;
                } else if (sym == SDLK_RETURN) {
                    LastASCII = '\r';
                } else if (sym == SDLK_ESCAPE) {
                    LastASCII = '\x1b';
                } else if (sym == SDLK_BACKSPACE) {
                    LastASCII = '\b';
                } else if (sym == SDLK_TAB) {
                    LastASCII = '\t';
                }
                break;
            }
            
            case SDL_EVENT_KEY_UP: {
                ScanCode sc = SDLToDOS(event.key.scancode);
                if (sc != sc_None) {
                    Keyboard[sc] = false;
                }
                break;
            }
            
            case SDL_EVENT_QUIT:
                Quit(NULL);
                break;
        }
    }
}

void IN_Startup(void)
{
    int i;
    for (i = 0; i < NumCodes; i++)
        Keyboard[i] = false;
    last_pump_ms = SDL_GetTicks();
    accumulated_ms = 0;
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
    (void)hook;
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
    IN_PumpEvents();
    
    ci->x = 0;
    ci->y = 0;
    ci->button0 = false;
    ci->button1 = false;
    ci->button2 = false;
    ci->button3 = false;
    ci->xaxis = motion_None;
    ci->yaxis = motion_None;
    ci->dir = dir_None;
    
    // Check keyboard arrow keys for cursor movement
    if (Keyboard[sc_UpArrow]) {
        ci->yaxis = motion_Up;
        ci->dir = dir_North;
    } else if (Keyboard[sc_DownArrow]) {
        ci->yaxis = motion_Down;
        ci->dir = dir_South;
    }
    
    if (Keyboard[sc_LeftArrow]) {
        ci->xaxis = motion_Left;
        if (ci->dir == dir_North) ci->dir = dir_NorthWest;
        else if (ci->dir == dir_South) ci->dir = dir_SouthWest;
        else ci->dir = dir_West;
    } else if (Keyboard[sc_RightArrow]) {
        ci->xaxis = motion_Right;
        if (ci->dir == dir_North) ci->dir = dir_NorthEast;
        else if (ci->dir == dir_South) ci->dir = dir_SouthEast;
        else ci->dir = dir_East;
    }
}

void IN_ReadControl(int player, ControlInfo *ci)
{
    IN_PumpEvents();
    
    ci->x = 0;
    ci->y = 0;
    ci->button0 = false;
    ci->button1 = false;
    ci->button2 = false;
    ci->button3 = false;
    ci->xaxis = motion_None;
    ci->yaxis = motion_None;
    ci->dir = dir_None;
    
    // Use KbdDefs for control mapping
    if (Keyboard[KbdDefs.upleft]) {
        ci->xaxis = motion_Left;
        ci->yaxis = motion_Up;
        ci->dir = dir_NorthWest;
    } else if (Keyboard[KbdDefs.upright]) {
        ci->xaxis = motion_Right;
        ci->yaxis = motion_Up;
        ci->dir = dir_NorthEast;
    } else if (Keyboard[KbdDefs.downleft]) {
        ci->xaxis = motion_Left;
        ci->yaxis = motion_Down;
        ci->dir = dir_SouthWest;
    } else if (Keyboard[KbdDefs.downright]) {
        ci->xaxis = motion_Right;
        ci->yaxis = motion_Down;
        ci->dir = dir_SouthEast;
    } else if (Keyboard[KbdDefs.up]) {
        ci->yaxis = motion_Up;
        ci->dir = dir_North;
    } else if (Keyboard[KbdDefs.down]) {
        ci->yaxis = motion_Down;
        ci->dir = dir_South;
    } else if (Keyboard[KbdDefs.left]) {
        ci->xaxis = motion_Left;
        ci->dir = dir_West;
    } else if (Keyboard[KbdDefs.right]) {
        ci->xaxis = motion_Right;
        ci->dir = dir_East;
    }
    
    ci->button0 = Keyboard[KbdDefs.button0];
    ci->button1 = Keyboard[KbdDefs.button1];
}

void IN_SetControlType(int player, ControlType type)
{
    Controls[player] = type;
}

void IN_GetJoyAbs(word joy, word *xp, word *yp)
{
    (void)joy;
    *xp = 0;
    *yp = 0;
}

void IN_SetupJoy(word joy, word minx, word maxx, word miny, word maxy)
{
    (void)joy;
    (void)minx;
    (void)maxx;
    (void)miny;
    (void)maxy;
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
    IN_StartAck();
    while (!IN_CheckAck())
        ;
}

void IN_AckBack(void)
{
    IN_ClearKeysDown();
}

boolean IN_UserInput(longword delay)
{
    longword lasttime = TimeCount;
    IN_StartAck();
    do {
        if (IN_CheckAck())
            return true;
    } while (TimeCount - lasttime < delay);
    return false;
}

char IN_WaitForASCII(void)
{
    char c;
    IN_ClearKeysDown();
    do {
        IN_PumpEvents();
    } while (!(c = LastASCII));
    IN_ClearKeysDown();
    return c;
}

ScanCode IN_WaitForKey(void)
{
    ScanCode sc;
    IN_ClearKeysDown();
    do {
        IN_PumpEvents();
    } while (!(sc = LastScan));
    IN_ClearKeysDown();
    return sc;
}

word IN_GetJoyButtonsDB(word joy)
{
    (void)joy;
    return 0;
}

byte *IN_GetScanName(ScanCode sc)
{
    static char name[16];
    switch (sc) {
        case sc_None: return (byte *)"None";
        case sc_Return: return (byte *)"Enter";
        case sc_Escape: return (byte *)"Esc";
        case sc_Space: return (byte *)"Space";
        case sc_BackSpace: return (byte *)"BackSp";
        case sc_Tab: return (byte *)"Tab";
        case sc_Alt: return (byte *)"Alt";
        case sc_Control: return (byte *)"Ctrl";
        case sc_LShift: return (byte *)"LShift";
        case sc_RShift: return (byte *)"RShift";
        case sc_UpArrow: return (byte *)"Up";
        case sc_DownArrow: return (byte *)"Down";
        case sc_LeftArrow: return (byte *)"Left";
        case sc_RightArrow: return (byte *)"Right";
        case sc_Insert: return (byte *)"Ins";
        case sc_Delete: return (byte *)"Del";
        case sc_Home: return (byte *)"Home";
        case sc_End: return (byte *)"End";
        case sc_PgUp: return (byte *)"PgUp";
        case sc_PgDn: return (byte *)"PgDn";
        case sc_F1: return (byte *)"F1";
        case sc_F2: return (byte *)"F2";
        case sc_F3: return (byte *)"F3";
        case sc_F4: return (byte *)"F4";
        case sc_F5: return (byte *)"F5";
        case sc_F6: return (byte *)"F6";
        case sc_F7: return (byte *)"F7";
        case sc_F8: return (byte *)"F8";
        case sc_F9: return (byte *)"F9";
        case sc_F10: return (byte *)"F10";
        case sc_F11: return (byte *)"F11";
        case sc_F12: return (byte *)"F12";
        default:
            if (sc >= sc_A && sc <= sc_Z) {
                name[0] = 'A' + (sc - sc_A);
                name[1] = '\0';
                return (byte *)name;
            }
            if (sc >= sc_1 && sc <= sc_0) {
                name[0] = '0' + ((sc - sc_1 + 1) % 10);
                name[1] = '\0';
                return (byte *)name;
            }
            return (byte *)"?";
    }
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
    (void)joy;
    *dx = 0;
    *dy = 0;
}

void IN_StartAck(void)
{
    unsigned i;
    IN_ClearKeysDown();
    memset(btnstate, 0, sizeof(btnstate));
    for (i = 0; i < 8; i++)
        if (IN_JoyButtons() & (1 << i))
            btnstate[i] = true;
}

boolean IN_CheckAck(void)
{
    unsigned i;
    
    IN_PumpEvents();
    
    if (LastScan)
        return true;
    
    for (i = 0; i < 8; i++) {
        if (IN_JoyButtons() & (1 << i)) {
            if (!btnstate[i])
                return true;
        } else {
            btnstate[i] = false;
        }
    }
    
    return false;
}
