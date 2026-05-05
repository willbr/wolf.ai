// ID_SD.C - SDL3 stub implementation

#include "ID_HEADS.H"

boolean		AdLibPresent = false;
boolean		SoundSourcePresent = false;
boolean		SoundBlasterPresent = false;
boolean		NeedsMusic = false;
boolean		SoundPositioned = false;
SDMode		SoundMode = sdm_Off;
SDSMode		DigiMode = sds_Off;
SMMode		MusicMode = smm_Off;
boolean		DigiPlaying = false;
int			DigiMap[LASTSOUND];
word			NumDigi = 0;
word			*DigiList = NULL;
longword	TimeCount = 0;

static void (*UserHook)(void) = NULL;

void SD_Startup(void)
{
}

void SD_Shutdown(void)
{
}

void SD_Default(boolean gotit, SDMode sd, SMMode sm)
{
	SoundMode = sdm_Off;
	MusicMode = smm_Off;
}

boolean SD_SetSoundMode(SDMode mode)
{
	SoundMode = mode;
	return true;
}

boolean SD_SetMusicMode(SMMode mode)
{
	MusicMode = mode;
	return true;
}

void SD_PositionSound(int leftvol, int rightvol)
{
}

boolean SD_PlaySound(soundnames sound)
{
	return false;
}

void SD_SetPosition(int leftvol, int rightvol)
{
}

void SD_StopSound(void)
{
	DigiPlaying = false;
}

void SD_WaitSoundDone(void)
{
}

word SD_SoundPlaying(void)
{
	return 0;
}

void SD_StartMusic(MusicGroup far *music)
{
}

void SD_MusicOn(void)
{
}

void SD_MusicOff(void)
{
}

boolean SD_MusicPlaying(void)
{
	return false;
}

void SD_FadeOutMusic(void)
{
}

void SD_SetUserHook(void (*hook)(void))
{
	UserHook = hook;
}

void SD_SetDigiDevice(SDSMode mode)
{
	DigiMode = mode;
}

void SD_PlayDigitized(word which, int leftpos, int rightpos)
{
}

void SD_StopDigitized(void)
{
	DigiPlaying = false;
}

void SD_Poll(void)
{
}

void alOut(byte n, byte b)
{
}
