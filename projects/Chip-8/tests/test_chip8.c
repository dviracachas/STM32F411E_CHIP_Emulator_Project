// tests/test_chip8.c
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../core/chip8.h"

#ifdef NDEBUG
#error "Tests must be built without NDEBUG, or assert() does nothing"
#endif

static void test_init_state(void)
{
    Chip8 c;
    memset(&c, 0xFF, sizeof c); // Fill with 0xFF
    chip8_init(&c);
    // Assert all initialized parameters
    assert(c.pc == CHIP8_PROG_START);
    assert(c.sp == 0);
    assert(c.i == 0);
    assert(c.delay_timer == 0);
    assert(c.sound_timer == 0);
    for (int n = 0; n < 16; n++) {
        assert(c.v[n] == 0);
        assert(c.keys[n] == false);
    }
    for (size_t n = 0; n < sizeof c.display; n++) {
        assert(c.display[n] == 0);
    }
}

static void test_init_loads_font(void)
{
    Chip8 c;
    chip8_init(&c);

    // Assert values from digit '0' (F0 90 90 90 F0), digit 'F' (index 15) ends with 80
    assert(c.memory[CHIP8_FONT_START + 0] == 0xF0);
    assert(c.memory[CHIP8_FONT_START + 1] == 0x90);
    assert(c.memory[CHIP8_FONT_START + 4] == 0xF0);
    assert(c.memory[CHIP8_FONT_START + 15 * 5 + 4] == 0x80);
    // Assert that program is untouched
    assert(c.memory[CHIP8_PROG_START] == 0);
}

static void test_load_rom_copies_data(void)
{
    Chip8 c;
    chip8_init(&c);

    const uint8_t rom[] = {0x12, 0x34, 0xAB, 0xCD};
    assert(chip8_load_rom(&c, rom, sizeof rom) == CHIP8_OK);
    // Assert written bytes with nothing written past or before limits
    assert(c.memory[CHIP8_PROG_START + 0] == 0x12);
    assert(c.memory[CHIP8_PROG_START + 1] == 0x34);
    assert(c.memory[CHIP8_PROG_START + 2] == 0xAB);
    assert(c.memory[CHIP8_PROG_START + 3] == 0xCD);
    assert(c.memory[CHIP8_PROG_START + 4] == 0);
    assert(c.memory[CHIP8_PROG_START - 1] == 0);
}

static void test_load_rom_max_size_accepted(void)
{
    Chip8 c;
    chip8_init(&c);

    static uint8_t rom[CHIP8_MEM_SIZE - CHIP8_PROG_START];
    memset(rom, 0xAA, sizeof rom);
    // Assert written bytes at the limits
    assert(chip8_load_rom(&c, rom, sizeof rom) == CHIP8_OK);
    assert(c.memory[CHIP8_PROG_START] == 0xAA);
    assert(c.memory[CHIP8_MEM_SIZE - 1] == 0xAA);
}

static void test_load_rom_too_big_rejected(void)
{
    Chip8 c;
    chip8_init(&c);

    static uint8_t rom[CHIP8_MEM_SIZE - CHIP8_PROG_START + 1]; // 1 byte extra
    memset(rom, 0xAA, sizeof rom);
    // Assert error and no written bytes
    assert(chip8_load_rom(&c, rom, sizeof rom) == CHIP8_ERR_TOO_BIG);
    assert(c.memory[CHIP8_PROG_START] == 0);
    assert(c.memory[CHIP8_MEM_SIZE - 1] == 0);
}

static void test_load_rom_empty(void)
{
    Chip8 c;
    chip8_init(&c);

    assert(chip8_load_rom(&c, NULL, 0) == CHIP8_OK);
    assert(c.memory[CHIP8_PROG_START] == 0);
}

static void test_set_key(void)
{
    Chip8 c;
    chip8_init(&c);

    chip8_set_key(&c, 5, true);
    assert(c.keys[5] == true);
    assert(c.keys[4] == false && c.keys[6] == false);

    chip8_set_key(&c, 5, false);
    assert(c.keys[5] == false);

    chip8_set_key(&c, 0, true);
    chip8_set_key(&c, 15, true);
    assert(c.keys[0] == true && c.keys[15] == true);
}

static void test_set_key_out_of_range_ignored(void)
{
    Chip8 c;
    chip8_init(&c);

    chip8_set_key(&c, 16, true);
    chip8_set_key(&c, 255, true);
    for (int n = 0; n < 16; n++) {
        assert(c.keys[n] == false);
    }
}

int main(void)
{
    test_init_state();
    test_init_loads_font();
    test_load_rom_copies_data();
    test_load_rom_max_size_accepted();
    test_load_rom_too_big_rejected();
    test_load_rom_empty();
    test_set_key();
    test_set_key_out_of_range_ignored();

    puts("Success. All tests passed!");
    return 0;
}