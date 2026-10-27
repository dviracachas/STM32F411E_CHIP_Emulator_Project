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

static Chip8Status exec(Chip8 *c, uint16_t op)
{
    put_op(c, op);
    return chip8_step(c);
}

static void test_op_00E0(void)
{
    Chip8 c;
    chip8_init(&c);
    memset(c.display, 1, sizeof c.display);
    assert(exec(&c, 0x00E0) == CHIP8_OK);
    for (size_t i = 0; i < sizeof(c.display); i++) {
        assert(c.display[i] == false);
    }
    assert(c.pc == CHIP8_PROG_START + 2);
}

static void test_op_1nnn(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0x1D08) == CHIP8_OK);
    assert(c.pc == 0xD08);
}

static void test_op_6xnn(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0x6D08) == CHIP8_OK);
    assert(c.v[0xD] == 0x08);
    assert(c.pc == CHIP8_PROG_START + 2);
}

static void test_op_7xnn(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0x7DFF) == CHIP8_OK);
    assert(c.v[0xD] == 0xFF);
    assert(c.pc == CHIP8_PROG_START + 2);
    assert(exec(&c, 0x7D02) == CHIP8_OK);
    assert(c.v[0xD] == 0x01);
    assert(c.v[0xC] == 0x00);
    assert(c.pc == CHIP8_PROG_START + 4);
}

static void test_op_annn(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0xAD08) == CHIP8_OK);
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
    assert(exec(&c, 0xD015) == CHIP8_OK);
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
    assert(exec(&c, 0xD015) == CHIP8_OK);
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
    assert(exec(&c, 0xD015) == CHIP8_OK);
    assert(c.v[0xF] == 0);
}

static unsigned skip_offset(uint16_t op, uint8_t v2, uint8_t v3)
{
    Chip8 c;
    chip8_init(&c);
    c.v[2] = v2;
    c.v[3] = v3;
    assert(exec(&c, op) == CHIP8_OK);
    return c.pc - CHIP8_PROG_START;
}

static void test_call_and_return(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0x2345) == CHIP8_OK);
    assert(c.pc == 0x345);
    assert(c.sp == 1);
    assert(c.stack[0] == CHIP8_PROG_START + 2);
    assert(exec(&c, 0x00EE) == CHIP8_OK);
    assert(c.pc == CHIP8_PROG_START + 2);
    assert(c.sp == 0);
}

static void test_stack_overflow(void)
{
    Chip8 c;
    chip8_init(&c);
    for (int n = 0; n < 16; n++) {
        assert(exec(&c, 0x2300) == CHIP8_OK);
    }
    assert(exec(&c, 0x2300) == CHIP8_ERR_STACK);
}

static void test_stack_underflow(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0x00EE) == CHIP8_ERR_STACK);
}

static void test_skip_3xnn(void)
{
    assert(skip_offset(0x3242, 0x42, 0) == 4);
    assert(skip_offset(0x3242, 0x43, 0) == 2);
}

static void test_skip_4xnn(void)
{
    assert(skip_offset(0x4242, 0x43, 0) == 4);
    assert(skip_offset(0x4242, 0x42, 0) == 2);
}

static void test_skip_5xy0(void)
{
    assert(skip_offset(0x5230, 7, 7) == 4);
    assert(skip_offset(0x5230, 7, 8) == 2);
}

static void test_skip_9xy0(void)
{
    assert(skip_offset(0x9230, 7, 8) == 4);
    assert(skip_offset(0x9230, 7, 7) == 2);
}

static void test_skip_bad_last_nibble(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0x5231) == CHIP8_ERR_BAD_OPCODE);
    chip8_init(&c);
    assert(exec(&c, 0x9231) == CHIP8_ERR_BAD_OPCODE);
}

static void test_8xy0_to_8xy3(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[1] = 0xF0;
    c.v[2] = 0x3C;
    assert(exec(&c, 0x8120) == CHIP8_OK);
    assert(c.v[1] == 0x3C);

    c.v[1] = 0xF0;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8121) == CHIP8_OK);
    assert(c.v[1] == 0xFC);

    c.v[1] = 0xF0;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8122) == CHIP8_OK);
    assert(c.v[1] == 0x30);

    c.v[1] = 0xF0;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8123) == CHIP8_OK);
    assert(c.v[1] == 0xCC);
}

static void test_8xy4_add(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[1] = 1;
    c.v[2] = 2;
    assert(exec(&c, 0x8124) == CHIP8_OK);
    assert(c.v[1] == 3 && c.v[0xF] == 0);

    c.v[1] = 0xFF;
    c.v[2] = 2;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8124) == CHIP8_OK);
    assert(c.v[1] == 1 && c.v[0xF] == 1);
}

static void test_8xy4_vf_as_target(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[0xF] = 200;
    c.v[1] = 100;
    assert(exec(&c, 0x8F14) == CHIP8_OK);
    assert(c.v[0xF] == 1);

    c.v[0xF] = 1;
    c.v[1] = 2;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8F14) == CHIP8_OK);
    assert(c.v[0xF] == 0);
}

static void test_8xy5_sub(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[1] = 5;
    c.v[2] = 3;
    assert(exec(&c, 0x8125) == CHIP8_OK);
    assert(c.v[1] == 2 && c.v[0xF] == 1);

    c.v[1] = 3;
    c.v[2] = 5;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8125) == CHIP8_OK);
    assert(c.v[1] == 0xFE && c.v[0xF] == 0);

    c.v[1] = 4;
    c.v[2] = 4;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8125) == CHIP8_OK);
    assert(c.v[1] == 0 && c.v[0xF] == 1);
}

static void test_8xy7_subn(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[1] = 3;
    c.v[2] = 5;
    assert(exec(&c, 0x8127) == CHIP8_OK);
    assert(c.v[1] == 2 && c.v[0xF] == 1);

    c.v[1] = 5;
    c.v[2] = 3;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8127) == CHIP8_OK);
    assert(c.v[1] == 0xFE && c.v[0xF] == 0);

    c.v[1] = 4;
    c.v[2] = 4;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8127) == CHIP8_OK);
    assert(c.v[1] == 0 && c.v[0xF] == 1);
}

static void test_8xy6_shift_right(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[1] = 0x81;
    assert(exec(&c, 0x8126) == CHIP8_OK);
    assert(c.v[1] == 0x40 && c.v[0xF] == 1);

    c.v[1] = 0x80;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x8126) == CHIP8_OK);
    assert(c.v[1] == 0x40 && c.v[0xF] == 0);
}

static void test_8xye_shift_left(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[1] = 0x81;
    assert(exec(&c, 0x812E) == CHIP8_OK);
    assert(c.v[1] == 0x02 && c.v[0xF] == 1);

    c.v[1] = 0x01;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0x812E) == CHIP8_OK);
    assert(c.v[1] == 0x02 && c.v[0xF] == 0);
}

static void test_8xy_unknown_subop(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0x812F) == CHIP8_ERR_BAD_OPCODE);
}

static void test_bnnn(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[0] = 5;
    assert(exec(&c, 0xB300) == CHIP8_OK);
    assert(c.pc == 0x305);
}

static void test_cxnn(void)
{
    Chip8 a, b;
    chip8_init(&a);
    chip8_init(&b);
    a.rng_state = b.rng_state = 12345;
    assert(exec(&a, 0xC1FF) == CHIP8_OK);
    assert(exec(&b, 0xC1FF) == CHIP8_OK);
    assert(a.v[1] == b.v[1]);

    chip8_init(&a);
    for (int n = 0; n < 50; n++) {
        a.pc = CHIP8_PROG_START;
        assert(exec(&a, 0xC20F) == CHIP8_OK);
        assert((a.v[2] & 0xF0) == 0);
        a.pc = CHIP8_PROG_START;
        assert(exec(&a, 0xC300) == CHIP8_OK);
        assert(a.v[3] == 0);
    }
}

static void test_fx1e(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = 0x100;
    c.v[3] = 0x10;
    assert(exec(&c, 0xF31E) == CHIP8_OK);
    assert(c.i == 0x110);
}

static void test_fx29(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[2] = 0xA;
    assert(exec(&c, 0xF229) == CHIP8_OK);
    assert(c.i == CHIP8_FONT_START + 50);
}

static void test_fx33(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = 0x300;
    c.v[4] = 156;
    assert(exec(&c, 0xF433) == CHIP8_OK);
    assert(c.memory[0x300] == 1 && c.memory[0x301] == 5 && c.memory[0x302] == 6);

    c.v[4] = 0;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0xF433) == CHIP8_OK);
    assert(c.memory[0x300] == 0 && c.memory[0x301] == 0 && c.memory[0x302] == 0);

    c.v[4] = 255;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0xF433) == CHIP8_OK);
    assert(c.memory[0x300] == 2 && c.memory[0x301] == 5 && c.memory[0x302] == 5);
}

static void test_fx33_bounds(void)
{
    Chip8 c;
    chip8_init(&c);
    c.v[4] = 123;
    c.i = CHIP8_MEM_SIZE - 3;
    assert(exec(&c, 0xF433) == CHIP8_OK);
    assert(c.memory[CHIP8_MEM_SIZE - 1] == 3);

    c.i = CHIP8_MEM_SIZE - 2;
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0xF433) == CHIP8_ERR_MEMORY);
}

static void test_fx55_fx65_roundtrip(void)
{
    Chip8 c;
    chip8_init(&c);
    for (int n = 0; n < 16; n++) {
        c.v[n] = (uint8_t)(n + 1);
    }
    c.i = 0x300;
    assert(exec(&c, 0xF355) == CHIP8_OK);
    for (int n = 0; n < 4; n++) {
        assert(c.memory[0x300 + n] == n + 1);
    }
    assert(c.memory[0x304] == 0);
    assert(c.i == 0x300);

    for (int n = 0; n < 16; n++) {
        c.v[n] = 0;
    }
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0xF365) == CHIP8_OK);
    for (int n = 0; n < 4; n++) {
        assert(c.v[n] == n + 1);
    }
    assert(c.v[4] == 0);
}

static void test_fx55_fx65_all_registers(void)
{
    Chip8 c;
    chip8_init(&c);
    for (int n = 0; n < 16; n++) {
        c.v[n] = (uint8_t)(0xA0 + n);
    }
    c.i = 0x300;
    assert(exec(&c, 0xFF55) == CHIP8_OK);
    assert(c.memory[0x30F] == 0xAF);

    for (int n = 0; n < 16; n++) {
        c.v[n] = 0;
    }
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0xFF65) == CHIP8_OK);
    for (int n = 0; n < 16; n++) {
        assert(c.v[n] == 0xA0 + n);
    }
}

static void test_fx55_fx65_bounds(void)
{
    Chip8 c;
    chip8_init(&c);
    c.i = CHIP8_MEM_SIZE - 10;
    assert(exec(&c, 0xFF55) == CHIP8_ERR_MEMORY);
    c.pc = CHIP8_PROG_START;
    assert(exec(&c, 0xFF65) == CHIP8_ERR_MEMORY);
}

static void test_op_unknown(void)
{
    Chip8 c;
    chip8_init(&c);
    assert(exec(&c, 0xE000) == CHIP8_ERR_BAD_OPCODE);
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
    test_call_and_return();
    test_stack_overflow();
    test_stack_underflow();
    test_skip_3xnn();
    test_skip_4xnn();
    test_skip_5xy0();
    test_skip_9xy0();
    test_skip_bad_last_nibble();
    test_8xy0_to_8xy3();
    test_8xy4_add();
    test_8xy4_vf_as_target();
    test_8xy5_sub();
    test_8xy7_subn();
    test_8xy6_shift_right();
    test_8xye_shift_left();
    test_8xy_unknown_subop();
    test_bnnn();
    test_cxnn();
    test_fx1e();
    test_fx29();
    test_fx33();
    test_fx33_bounds();
    test_fx55_fx65_roundtrip();
    test_fx55_fx65_all_registers();
    test_fx55_fx65_bounds();
    test_op_unknown();

    puts("Success. All tests passed!");
    return 0;
}