#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include "../core/chip8.h"
#include <stdatomic.h>

#define SCALE 10

#define CPU_HZ 700
#define TIMER_HZ 60

#define SAMPLE_RATE 44100
#define TONE_HZ 440
#define AMPLITUDE 3000

// Assign Chip-8 keypad with PC keys
static const SDL_Scancode keymap[16] = {
    SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, // 0 1 2 3
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A, // 4 5 6 7
    SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C, // 8 9 A B
    SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V  // C D E F
};

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

typedef struct {
    atomic_bool beeping;
    uint32_t phase;
} Audio;

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    Audio *a = userdata;
    int16_t *out = (int16_t *)stream;
    int n = len / (int)sizeof(int16_t);
    const uint32_t period = SAMPLE_RATE / TONE_HZ;

    for (int k = 0; k < n; k++) {
        if (atomic_load(&a->beeping)) {
            out[k] = (a->phase < period / 2) ? AMPLITUDE : -AMPLITUDE;
            a->phase = (a->phase + 1) % period;
        } else {
            out[k] = 0;
            a->phase = 0;
        }
    }
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // Setting up for beeping
    static Audio audio;
    atomic_init(&audio.beeping, false);

    SDL_AudioSpec want = {0}, have;
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_callback;
    want.userdata = &audio;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        fprintf(stderr, "audio: %s (continuing without sound)\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(dev, 0);
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

    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t last = SDL_GetPerformanceCounter();
    double cpu_acc = 0.0;
    double timer_acc = 0.0;

    bool running = true;
    while (running) {
        // Detect events to quit
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                fprintf(stderr, "quitting because of this event\n");
                running = false;
            }
        }
        // Read the keyboard state
        const uint8_t *state = SDL_GetKeyboardState(NULL);
        for (uint8_t k = 0; k < 16; k++) {
            chip8_set_key(&chip, k, state[keymap[k]]);
        }

        uint64_t now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)freq;
        last = now;
        if (dt > 0.1)
            dt = 0.1;
        cpu_acc += dt;
        timer_acc += dt;

        // Process instructions
        while (cpu_acc >= 1.0 / CPU_HZ) {
            Chip8Status s = chip8_step(&chip);
            if (s != CHIP8_OK) {
                fprintf(stderr, "stopped at pc=0x%03X, status %d\n", chip.pc, s);
                running = false;
                break;
            }
            cpu_acc -= 1.0 / CPU_HZ;
        }
        while (timer_acc >= 1.0 / TIMER_HZ) {
            chip8_tick_timers(&chip);
            timer_acc -= 1.0 / TIMER_HZ;
        }

        atomic_store(&audio.beeping, chip.sound_timer > 0);
        draw(ren, &chip);
        SDL_Delay(1);
    }
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    if (dev != 0)
        SDL_CloseAudioDevice(dev);
    SDL_Quit();
    fprintf(stderr, "Main loop exited\n");
    return 0;
}