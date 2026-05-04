#include <SDL3/SDL.h>
#include <stdio.h>

// SDL3 Audio backend stub
// Prints console messages for sound triggers

void SDL3_Audio_Init(void) {
    printf("SDL3_Audio_Init: stub\n");
}

void SDL3_Audio_Shutdown(void) {
    printf("SDL3_Audio_Shutdown: stub\n");
}

void SDL3_Audio_PlaySound(int soundnum) {
    printf("[AUDIO] PlaySound(%d)\n", soundnum);
}

void SDL3_Audio_StartMusic(int musicnum) {
    printf("[AUDIO] StartMusic(%d)\n", musicnum);
}

void SDL3_Audio_StopMusic(void) {
    printf("[AUDIO] StopMusic()\n");
}
