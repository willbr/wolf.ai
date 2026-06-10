// ID_SD.C - Wolf3D sound manager on the SDL3 + Nuked-OPL3 backend (sdl3_sd.c).
//
// This layer is the game-facing SD_* API. It owns the sound/music mode state,
// pulls the right bytes out of the cached AUDIOT chunks (parsed by offset, not
// struct cast — the DOS structs use 4-byte longs that don't match 64-bit), and
// hands raw data to the audio backend.

#include "ID_HEADS.H"
#include "id_ca.h"
#include "id_pm.h"
#include <stdlib.h>

// ---- audio backend (sdl3_sd.c) ----
extern void SDA_Startup(void);
extern void SDA_Shutdown(void);
extern void SDA_MusicOn(const byte *data, int chunklen);
extern void SDA_MusicOff(void);
extern int  SDA_MusicPlaying(void);
extern void SDA_PlayAdLib(const byte *inst, byte block, const byte *data, int len, int priority);
extern void SDA_PlayDigi(const byte *pcm, int len, int leftvol, int rightvol);
extern void SDA_StopSound(void);
extern void SDA_StopDigi(void);
extern int  SDA_SoundPlaying(void);
extern void SDL3_Delay(unsigned int ms);

boolean		AdLibPresent = true;		// emulated via Nuked-OPL3
boolean		SoundSourcePresent = false;
boolean		SoundBlasterPresent = true;	// digitized sounds (emulated)
boolean		NeedsMusic = false;
boolean		SoundPositioned = false;
SDMode		SoundMode = sdm_Off;
SDSMode		DigiMode = sds_Off;
SMMode		MusicMode = smm_Off;
boolean		DigiPlaying = false;
int			DigiMap[LASTSOUND];
word		NumDigi = 0;
word		*DigiList = NULL;
longword	TimeCount = 0;

static int	curPriority = 0;
static int	leftPos = 8, rightPos = 8;	// 0..15 panning, set by SD_PositionSound

static void (*UserHook)(void) = NULL;

// The digitized-sound directory: (page, length) pairs read from the VSWAP
// sound info page (the last page chunk). These are 16-bit fields in the file,
// NOT the 32-bit `word` type here, so read them as uint16_t. Sound i's PCM
// (8-bit unsigned mono @7000 Hz) lives at PM_GetPage(PMSoundStart+digiDir[i*2]).
static uint16_t *digiDir;

static void SD_SetupDigi(void)
{
	uint16_t *info = (uint16_t *)PM_GetPage(ChunksInFile - 1);
	if (!info)
		return;
	int maxpairs = (int)PM_GetPageSize(ChunksInFile - 1) / 4;

	// Count entries that fit in the available sound pages (matches original).
	int pg = PMSoundStart;
	int n = 0;
	for (n = 0; n < maxpairs; n++)
	{
		if (pg >= ChunksInFile - 1)
			break;
		pg += (info[n * 2 + 1] + (PMPageSize - 1)) / PMPageSize;
	}

	NumDigi = (word)n;
	digiDir = (uint16_t *)malloc(n * 2 * sizeof(uint16_t));
	memcpy(digiDir, info, n * 2 * sizeof(uint16_t));
}

void SD_Startup(void)
{
	SDA_Startup();
	for (int i = 0; i < LASTSOUND; i++)
		DigiMap[i] = -1;
	SD_SetupDigi();
}

void SD_Shutdown(void)
{
	SDA_StopSound();
	SDA_MusicOff();
	SDA_Shutdown();
}

void SD_Default(boolean gotit, SDMode sd, SMMode sm)
{
	if (!gotit)
	{
		sd = AdLibPresent ? sdm_AdLib : sdm_Off;
		sm = AdLibPresent ? smm_AdLib : smm_Off;
	}
	SD_SetSoundMode(sd);
	SD_SetMusicMode(sm);
}

boolean SD_SetSoundMode(SDMode mode)
{
	SoundMode = mode;
	return true;
}

boolean SD_SetMusicMode(SMMode mode)
{
	SD_MusicOff();
	MusicMode = mode;
	return true;
}

void SD_PositionSound(int leftvol, int rightvol)
{
	leftPos = leftvol;
	rightPos = rightvol;
	SoundPositioned = true;
}

void SD_SetPosition(int leftvol, int rightvol)
{
	SD_PositionSound(leftvol, rightvol);
}

boolean SD_PlaySound(soundnames sound)
{
	if (sound == (soundnames)-1)
		return false;

	boolean ispos = SoundPositioned;
	int lp = leftPos, rp = rightPos;
	SoundPositioned = false;

	// The AdLib sound chunk supplies the priority used for BOTH digi and AdLib
	// playback (parsed by offset; the DOS structs don't match 64-bit layout):
	//   0: longword length(4)  4: word priority(2)  6: Instrument(16)
	//   22: byte block          23: note data[length]
	int chunk = STARTADLIBSOUNDS + sound;
	byte *p = NULL;
	int priority = 0;
	if (chunk < numaudiochunks)
	{
		if (!audiosegs[chunk])
			CA_CacheAudioChunk(chunk);
		p = audiosegs[chunk];
		if (p)
			priority = p[4] | (p[5] << 8);
	}

	if (SDA_SoundPlaying() && priority < curPriority)
		return false;

	// Prefer the digitized version when available and enabled.
	if (DigiMode != sds_Off && DigiMap[sound] != -1)
	{
		SD_PlayDigitized(DigiMap[sound], ispos ? lp : 0, ispos ? rp : 0);
		curPriority = priority;
		SoundPositioned = ispos;
		return true;
	}

	if (SoundMode == sdm_Off || !p)
		return false;

	longword length = p[0] | (p[1] << 8) | ((longword)p[2] << 16) | ((longword)p[3] << 24);
	byte block = p[22];
	byte *data = p + 23;
	SDA_PlayAdLib(p + 6, block, data, (int)length, priority);
	curPriority = priority;
	return true;
}

void SD_StopSound(void)
{
	SDA_StopSound();
	curPriority = 0;
	DigiPlaying = false;
}

void SD_WaitSoundDone(void)
{
	int guard = 0;
	while (SDA_SoundPlaying() && guard++ < 2000)
		SDL3_Delay(1);
}

word SD_SoundPlaying(void)
{
	return SDA_SoundPlaying() ? (word)curPriority : 0;
}

void SD_StartMusic(MusicGroup far *music)
{
	SDA_MusicOff();
	if (MusicMode != smm_AdLib || !music)
		return;
	byte *p = (byte *)music;
	int imflen = p[0] | (p[1] << 8);
	SDA_MusicOn(p, imflen + 2);
}

void SD_MusicOn(void)
{
}

void SD_MusicOff(void)
{
	SDA_MusicOff();
}

boolean SD_MusicPlaying(void)
{
	return SDA_MusicPlaying() ? true : false;
}

void SD_FadeOutMusic(void)
{
	SDA_MusicOff();		// no gradual fade; just stop
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
	if (DigiMode == sds_Off || !digiDir || which >= NumDigi)
		return;

	int page = digiDir[which * 2 + 0];
	int len  = digiDir[which * 2 + 1];
	byte *pcm = (byte *)PM_GetPage(PMSoundStart + page);
	if (!pcm)
		return;

	// leftpos/rightpos are 0..15 attenuation (0 = loudest); pack into 0..255.
	if (leftpos  < 0) leftpos  = 0; else if (leftpos  > 15) leftpos  = 15;
	if (rightpos < 0) rightpos = 0; else if (rightpos > 15) rightpos = 15;
	SDA_PlayDigi(pcm, len, (15 - leftpos) * 17, (15 - rightpos) * 17);
	DigiPlaying = true;
}

void SD_StopDigitized(void)
{
	SDA_StopDigi();
	DigiPlaying = false;
}

void SD_Poll(void)
{
}

void alOut(byte n, byte b)
{
	(void)n; (void)b;
}
