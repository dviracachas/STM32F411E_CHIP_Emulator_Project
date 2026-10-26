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

static void put_op(Chip8 *c, uint16_t op)
{
    c->memory[c->pc] = (uint8_t)(op >> 8);
    c->memory[c->pc + 1] = (uint8_t)(op & 0xFF);
}

static void test_op_00E0(void)
{
    Chip8 c;
    chip8_init(&c);
    memset(c.display, 1, sizeof c.display);
    put_op(&c, 0x00E0);
    assert(chip8_step(&c) == CHIP8_OK);
    for (size_t i = 0; i < sizeof(c.display); i++) {
        assert(c.display[i] == false);
    }
    assert(c.pc == CHIP8_PROG_START + 2);
}

static void test_op_1nnn(void)
{
    Chip8 c;
    chip8_init(&c);
    put_op(&c, 0x1D08);
    assert(chip8_step(&c) == CHIP8_OK);
    assert(c.pc == 0xD08);
}

static void test_op_6xnn(void)
{
    Chip8 c;
    chip8_init(&c);
    put_op(&c, 0x6D08);
    assert(chip8_step(&c) == CHIP8_OK);
    assert(c.v[0xD] == 0x08);
    assert(c.pc == CHIP8_PROG_START + 2);
}

static void test_op_7xnn(void)
{
    Chip8 c;
    chip8_init(&c);
    put_op(&c, 0x7DFF);
    assert(chip8_step(&c) == CHIP8_OK);
    assert(c.v[0xD] == 0xFF);
    assert(c.pc == CHIP8_PROG_START + 2);
    put_op(&c, 0x7D02);
    assert(chip8_step(&c) == CHIP8_OK);
    assert(c.v[0xD] == 0x01);
    assert(c.v[0xC] == 0x00);
    assert(c.pc == CHIP8_PROG_START + 4);
}

static void test_op_annn(void)
{
    Chip8 c;
    chip8_init(&c);
    put_op(&c, 0xAD08);
    assert(chip8_step(&c) == CHIP8_OK);
    assert(c.i == 0xD08);
    assert(c.pc == CHIP8_PROG_START + 2);
}

static void test_op_dxyn_draws_digit_zero(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = CHIP8_FONT_START; // Use the zero from font
    c.v[0] = 0;
    c.v[1] = 0;
    put_op(&c, 0xD015);
    assert(chip8_step(&c) == CHIP8_OK);
    assert(c.display[0] && c.display[1] && c.display[2] && c.display[3]);
    assert(!c.display[4] && !c.display[7]);
    assert(c.display[64 + 0] && !c.display[64 + 1] && !c.display[64 + 2] && c.display[64 + 3]);
    assert(c.display[4 * 64 + 0] && c.display[4 * 64 + 3]);
    for (int x = 0; x < 8; x++) {
        assert(!c.display[5 * 64 + x]);
    }
    assert(c.v[0xF] == 0); /* no collision on a blank screen */
    assert(c.pc == CHIP8_PROG_START + 2);
}

static void test_op_dxyn_xor_erases_and_sets_collision(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = CHIP8_FONT_START;
    put_op(&c, 0xD015);
    assert(chip8_step(&c) == CHIP8_OK);
    c.pc = CHIP8_PROG_START;
    assert(chip8_step(&c) == CHIP8_OK);
    for (size_t n = 0; n < sizeof c.display; n++) {
        assert(c.display[n] == 0);
    }
    assert(c.v[0xF] == 1);
}

static void test_op_dxyn_vf_reset_when_no_collision(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = CHIP8_FONT_START;
    c.v[0xF] = 1;
    put_op(&c, 0xD015);
    assert(chip8_step(&c) == CHIP8_OK);
    assert(c.v[0xF] == 0);
}

static void test_op_unknown(void)
{
    Chip8 c;
    chip8_init(&c);
    put_op(&c, 0x8000);
    assert(chip8_step(&c) == CHIP8_ERR_BAD_OPCODE);
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

    test_op_00E0();
    test_op_1nnn();
    test_op_6xnn();
    test_op_7xnn();
    test_op_annn();
    test_op_dxyn_draws_digit_zero();
    test_op_dxyn_xor_erases_and_sets_collision();
    test_op_dxyn_vf_reset_when_no_collision();
    test_op_unknown();

    puts("Success. All tests passed!");
    return 0;
}