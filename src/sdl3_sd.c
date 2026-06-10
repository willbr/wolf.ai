// SDL3 + Nuked-OPL3 audio backend for Wolf3D.
//
// Owns the OPL2 (emulated via OPL3) chip, the SDL audio stream, and all the
// audio-thread state: the IMF music player (700 Hz), the AdLib sound-effect
// player (140 Hz), and the digitized PCM mixer (7000 Hz). The game-thread
// SD_* functions in id_sd.c hand us raw chunk data through the small interface
// declared at the bottom; everything timing-critical happens here on the audio
// thread, sample-accurately.

#include <SDL3/SDL.h>
#include <string.h>
#include "opl3.h"

#define OPL_RATE        44100
#define MUSIC_HZ        700
#define SOUND_HZ        140
#define DIGI_HZ         7000
#define MUSIC_TICK_SAMP (OPL_RATE / MUSIC_HZ)   // 63
#define SOUND_TICK_SAMP (OPL_RATE / SOUND_HZ)   // 315

// OPL register addresses (operator-relative for channel 0: mod=0, car=3)
#define alChar   0x20
#define alScale  0x40
#define alAttack 0x60
#define alSus    0x80
#define alWave   0xe0
#define alFreqL  0xa0
#define alFreqH  0xb0
#define alFeedCon 0xc0
#define alEffects 0xbd

static opl3_chip     chip;
static SDL_AudioStream *stream;
static int           audio_started;

// ---- music (IMF) state ----
static int            mus_len;      // bytes remaining in current pass
static const uint8_t *mus_start;    // for looping
static int            mus_total;
static int            mus_ptr;      // byte offset into mus_start
static int            mus_time;     // tick countdown until next event
static int            music_on;
static int            samp_to_music; // samples until next 700Hz tick

// ---- adlib sfx state ----
static const uint8_t *sfx_data;     // note bytes
static int            sfx_len;
static int            sfx_pos;
static uint8_t        sfx_block;    // precomputed alFreqH key-on value
static int            sfx_priority;
static int            samp_to_sound; // samples until next 140Hz tick

// ---- digitized sfx state ----
static const uint8_t *digi_data;
static int            digi_len;
static int            digi_pos;
static int            digi_acc;
static int            digi_lvol = 255, digi_rvol = 255;

static void AL(uint16_t reg, uint8_t val) { OPL3_WriteReg(&chip, reg, val); }

static void OPL_ClearAll(void)
{
	for (int i = 1; i <= 0xf5; i++)
		AL(i, 0);
	AL(0x01, 0x20);   // enable waveform select
	AL(alEffects, 0); // no rhythm, default vibrato/tremolo depth
}

// Load a 16-byte Wolf3D Instrument onto channel 0 (mod cell 0, car cell 3).
static void SDA_SetInstrument(const uint8_t *in)
{
	const uint8_t m = 0, c = 3;
	AL(alChar + m,   in[0]);   // mChar
	AL(alChar + c,   in[1]);   // cChar
	AL(alScale + m,  in[2]);   // mScale
	AL(alScale + c,  in[3]);   // cScale
	AL(alAttack + m, in[4]);   // mAttack
	AL(alAttack + c, in[5]);   // cAttack
	AL(alSus + m,    in[6]);   // mSus
	AL(alSus + c,    in[7]);   // cSus
	AL(alWave + m,   in[8]);   // mWave
	AL(alWave + c,   in[9]);   // cWave
	AL(alFeedCon,    in[10]);  // nConn (feedback/connection, channel 0)
}

// --- one 700 Hz music tick: emit all IMF events due this tick ---
static void MusicTick(void)
{
	if (!music_on || mus_total <= 0)
		return;
	if (--mus_time > 0)
		return;
	for (;;)
	{
		uint8_t reg = mus_start[mus_ptr + 0];
		uint8_t val = mus_start[mus_ptr + 1];
		int delay   = mus_start[mus_ptr + 2] | (mus_start[mus_ptr + 3] << 8);
		AL(reg, val);
		mus_ptr += 4;
		mus_len -= 4;
		if (mus_len <= 0)   // loop back to the start
		{
			mus_ptr = 0;
			mus_len = mus_total;
		}
		if (delay)
		{
			mus_time = delay;
			break;
		}
	}
}

// --- one 140 Hz sound tick: advance the AdLib note sequence ---
static void SoundTick(void)
{
	if (!sfx_data)
		return;
	uint8_t s = sfx_data[sfx_pos++];
	if (!s)
		AL(alFreqH, 0);          // key off
	else
	{
		AL(alFreqL, s);          // frequency low byte
		AL(alFreqH, sfx_block);  // block + key-on
	}
	if (sfx_pos >= sfx_len)
	{
		AL(alFreqH, 0);
		sfx_data = NULL;
		sfx_priority = 0;
	}
}

// Mix `frames` of digitized PCM (u8 mono @7000Hz) into a stereo s16 block.
static void MixDigi(int16_t *buf, int frames)
{
	if (!digi_data)
		return;
	for (int i = 0; i < frames && digi_data; i++)
	{
		int s = ((int)digi_data[digi_pos] - 128) * 64;   // u8 -> ~+-8192
		int l = buf[i * 2]     + s * digi_lvol / 256;
		int r = buf[i * 2 + 1] + s * digi_rvol / 256;
		if (l >  32767) l =  32767; else if (l < -32768) l = -32768;
		if (r >  32767) r =  32767; else if (r < -32768) r = -32768;
		buf[i * 2]     = (int16_t)l;
		buf[i * 2 + 1] = (int16_t)r;

		digi_acc += DIGI_HZ;
		while (digi_acc >= OPL_RATE)
		{
			digi_acc -= OPL_RATE;
			if (++digi_pos >= digi_len) { digi_data = NULL; break; }
		}
	}
}

static void SDLCALL AudioCallback(void *ud, SDL_AudioStream *str, int additional, int total)
{
	(void)ud; (void)total;
	int frames_needed = additional / (int)(2 * sizeof(int16_t));
	int16_t buf[512 * 2];

	while (frames_needed > 0)
	{
		// advance any ticks that are due right now
		if (samp_to_music <= 0) { MusicTick(); samp_to_music += MUSIC_TICK_SAMP; }
		if (samp_to_sound <= 0) { SoundTick(); samp_to_sound += SOUND_TICK_SAMP; }

		int n = frames_needed;
		if (n > samp_to_music) n = samp_to_music;
		if (n > samp_to_sound) n = samp_to_sound;
		if (n > 512) n = 512;

		OPL3_GenerateStream(&chip, buf, n);
		MixDigi(buf, n);
		SDL_PutAudioStreamData(str, buf, n * 2 * (int)sizeof(int16_t));

		frames_needed  -= n;
		samp_to_music  -= n;
		samp_to_sound  -= n;
	}
}

// ====================== interface used by id_sd.c ======================

void SDA_Startup(void)
{
	if (audio_started)
		return;
	OPL3_Reset(&chip, OPL_RATE);
	OPL_ClearAll();
	samp_to_music = MUSIC_TICK_SAMP;
	samp_to_sound = SOUND_TICK_SAMP;

	SDL_AudioSpec spec;
	spec.format = SDL_AUDIO_S16;
	spec.channels = 2;
	spec.freq = OPL_RATE;
	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, NULL);
	if (!stream)
	{
		SDL_Log("SDA_Startup: audio stream failed: %s", SDL_GetError());
		return;
	}
	SDL_ResumeAudioStreamDevice(stream);
	audio_started = 1;
}

void SDA_Shutdown(void)
{
	if (!audio_started)
		return;
	SDL_DestroyAudioStream(stream);
	stream = NULL;
	audio_started = 0;
}

static void Lock(void)   { if (stream) SDL_LockAudioStream(stream); }
static void Unlock(void) { if (stream) SDL_UnlockAudioStream(stream); }

// IMF data: `data` points at the 2-byte length header, full chunk length `chunklen`.
void SDA_MusicOn(const uint8_t *data, int chunklen)
{
	Lock();
	int imflen = data[0] | (data[1] << 8);     // length word
	if (imflen <= 0 || imflen > chunklen - 2)
		imflen = chunklen - 2;
	mus_start = data + 2;
	mus_total = imflen;
	mus_ptr   = 0;
	mus_len   = imflen;
	mus_time  = 1;
	music_on  = 1;
	Unlock();
}

void SDA_MusicOff(void)
{
	Lock();
	music_on = 0;
	mus_total = 0;
	// silence all channels' carriers
	for (int c = 0; c < 9; c++)
		AL(alFreqH + c, 0);
	Unlock();
}

int SDA_MusicPlaying(void) { return music_on; }

// AdLib sound: 16-byte instrument, octave block, note data + length, priority.
void SDA_PlayAdLib(const uint8_t *inst, uint8_t block, const uint8_t *data, int len, int priority)
{
	Lock();
	SDA_SetInstrument(inst);
	AL(alFreqL, 0);
	AL(alFreqH, 0);
	sfx_block    = ((block & 7) << 2) | 0x20;   // block in bits 2-4, key-on bit 5
	sfx_data     = data;
	sfx_len      = len;
	sfx_pos      = 0;
	sfx_priority = priority;
	samp_to_sound = SOUND_TICK_SAMP;
	Unlock();
}

void SDA_PlayDigi(const uint8_t *pcm, int len, int leftvol, int rightvol)
{
	Lock();
	digi_data = pcm;
	digi_len  = len;
	digi_pos  = 0;
	digi_acc  = 0;
	digi_lvol = leftvol;
	digi_rvol = rightvol;
	Unlock();
}

void SDA_StopSound(void)
{
	Lock();
	sfx_data = NULL;
	sfx_priority = 0;
	digi_data = NULL;
	AL(alFreqH, 0);
	Unlock();
}

void SDA_StopDigi(void)
{
	Lock();
	digi_data = NULL;
	Unlock();
}

int SDA_SoundPlaying(void)
{
	return (sfx_data || digi_data) ? sfx_priority + 1 : 0;
}
