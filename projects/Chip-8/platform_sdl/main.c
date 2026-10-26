#include <SDL2/SDL.h>
#include "../core/chip8.h"

#define SCALE 10

// Read file into buf
static int load_file(const char *path, uint8_t *buf, size_t cap, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        return -1;
    }
    *out_len = fread(buf, 1, cap, f);
    fclose(f);
    return 0;
}

// Draw a border and a diagonal to check the display path
/* static void test_pattern(Chip8 *c)
{
    for (unsigned x = 0; x < CHIP8_DISP_W; x++) {
        c->display[x] = 1;                                     // Top
        c->display[x + (CHIP8_DISP_H - 1) * CHIP8_DISP_W] = 1; // Bottom
    }
    for (unsigned y = 0; y < CHIP8_DISP_H; y++) {
        c->display[y * CHIP8_DISP_W] = 1;                      // Left
        c->display[(CHIP8_DISP_W - 1) + y * CHIP8_DISP_W] = 1; // Right
        c->display[y + y * CHIP8_DISP_W] = 1;                  // Diagonal
    }
} */

static void draw(SDL_Renderer *ren, const Chip8 *c)
{
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);

    for (unsigned y = 0; y < CHIP8_DISP_H; y++) {
        for (unsigned x = 0; x < CHIP8_DISP_W; x++) {
            if (c->display[x + y * CHIP8_DISP_W]) {
                SDL_Rect r = {(int)x * SCALE, (int)y * SCALE, SCALE, SCALE};
                SDL_RenderFillRect(ren, &r);
            }
        }
    }
    SDL_RenderPresent(ren);
}

int main(int argc, char **argv)
{
    static Chip8 chip;
    chip8_init(&chip);

    if (argc > 1) {
        static uint8_t rom[CHIP8_MEM_SIZE];
        size_t len = 0;
        if (load_file(argv[1], rom, sizeof rom, &len) != 0) {
            return 1;
        }
        if (chip8_load_rom(&chip, rom, len) != CHIP8_OK) {
            fprintf(stderr, "ROM too big (%zu bytes)\n", len);
            return 1;
        }
    }

    // test_pattern(&chip);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       CHIP8_DISP_W * SCALE, CHIP8_DISP_H * SCALE, 0);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
    if (win == NULL || ren == NULL) {
        fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        if (win) {
            SDL_DestroyWindow(win);
        }
        SDL_Quit();
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                fprintf(stderr, "quitting because of this event\n");
                running = false;
            }
        }
        for (int k = 0; k < 10; k++) {
            Chip8Status s = chip8_step(&chip);
            if (s != CHIP8_OK) {
                fprintf(stderr, "stopped at pc=0x%03X, status %d\n", chip.pc, s);
                running = false;
                break;
            }
        }
        draw(ren, &chip);
        SDL_Delay(16); // Approx 60 FPS
    }
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    fprintf(stderr, "Main loop exited\n");
    return 0;
}