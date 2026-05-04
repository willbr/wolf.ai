#include <SDL3/SDL.h>
#include <stdio.h>

// Forward declaration from wl_main.c
extern void wolf_main(void);

// Borland-style command line globals
int _argc;
char **_argv;

void SDL3_Delay(unsigned int ms) {
    SDL_Delay(ms);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    _argc = argc;
    _argv = argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    wolf_main();

    SDL_Quit();
    return 0;
}
