#include "roms.h"
#include "chip8.h"

#define ROM_MAX (CHIP8_MEM_SIZE - CHIP8_PROG_START)

static const uint8_t timer_rom[] = {
    0x60, 0x3C, // 0x200: V0 = 60 (Hz)
    0xF0, 0x15, // 0x202: delay_timer = V0
    0xF1, 0x07, // 0x204: V1 = delay_timer
    0x31, 0x00, // 0x206: skip next if V1 == 0
    0x12, 0x04, // 0x208: Jump to 0x204 (Wait for delay)
    0x62, 0x00, // 0x20A: V2 = 0
    0x63, 0x00, // 0x20C: V3 = 0
    0xA0, 0x91, // 0x20E: I = 0x91 (Font digit D)
    0xD2, 0x35, // 0x210: Draw 5 rows at (V2, V3)
    0x12, 0x12  // 0x212: Jump to itself (Repeat)
};

static const uint8_t ball_moving_rom[] = {
    0x60, 0x00, // 0x200: V0 = 0
    0x61, 0x0A, // 0x202: V1 = 10
    0xA2, 0x20, /// 0x204: I = 0x220 (Ball sprite)
    0xD0, 0x18, // 0x206: Draw 8 rows at (V0, V1)
    0x62, 0x02, // 0x208: V2 = 2
    0xF2, 0x15, // 0x20A: delay_timer = V2
    0xF3, 0x07, // 0x20C: V3 = delay_timer
    0x33, 0x00, // 0x20E: Skip next if V3 == 0
    0x12, 0x0C, // 0x210: Jump to 20C (Wait for delay)
    0xD0, 0x18, // 0x212: Erase (Draw again)
    0x70, 0x01, // 0x214: V0 += 1
    0x30, 0x40, // 0x216: Skip next if V0 == 64
    0x12, 0x06, // 0x218: Jump to 206
    0x60, 0x00, // 0x21A: V0 = 0 (Wrap around)
    0x12, 0x06, // 0x21C: Jump to 206
    0x00, 0x00, // 0x21E: Padding
    // At 0x220
    0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C // Ball sprite
};

#define ROM_CHECK(rom)                                                                             \
    _Static_assert(sizeof(rom) > 0 && sizeof(rom) <= ROM_MAX, #rom ": ROM size too large")

ROM_CHECK(timer_rom);
ROM_CHECK(ball_moving_rom);

static const rom_entry_t builtin[] = {
    {"1 second timer and render", timer_rom, sizeof timer_rom},
    {"Moving ball", ball_moving_rom, sizeof ball_moving_rom},
};

#define BUILTIN_COUNT (sizeof builtin / sizeof builtin[0])

size_t rom_count(void)
{
    return BUILTIN_COUNT + user_rom_count;
}

const rom_entry_t *rom_get(size_t i)
{
    if (i < BUILTIN_COUNT)
        return &builtin[i];
    i -= BUILTIN_COUNT;
    return (i < user_rom_count) ? &user_roms[i] : NULL;
}
