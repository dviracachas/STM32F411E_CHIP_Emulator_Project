#include "chip8.h"
#include <stdint.h>
#include <string.h>

static const uint8_t font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void chip8_init(Chip8 *c)
{
    memset(c, 0, sizeof *c);                                 // Fill with 0
    c->pc = CHIP8_PROG_START;                                // Program counter to start
    memcpy(&c->memory[CHIP8_FONT_START], font, sizeof font); // Copy font to memory
}

Chip8Status chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len)
{
    if (len > CHIP8_MEM_SIZE - CHIP8_PROG_START) // 4096 - 512 bytes
        return CHIP8_ERR_TOO_BIG;
    if (len > 0)
        memcpy(&c->memory[CHIP8_PROG_START], data, len);
    return CHIP8_OK;
}

void chip8_set_key(Chip8 *c, uint8_t key, bool pressed)
{
    if (key >= 16)
        return;
    c->keys[key] = pressed;
}

Chip8Status chip8_step(Chip8 *c)
{
    // Prevent reading outside memory
    if (c->pc > CHIP8_MEM_SIZE - 2) {
        return CHIP8_ERR_BAD_OPCODE;
    }

    // Read the instruction and advance pc
    uint16_t op = (uint16_t)(c->memory[c->pc] << 8) | c->memory[c->pc + 1];
    c->pc += 2;

    // Decode nibbles
    uint8_t x = (op >> 8) & 0xF; // 2nd
    uint8_t y = (op >> 4) & 0xF; // 3rd
    uint8_t n = op & 0xF;        // 4th
    uint8_t nn = op & 0xFF;      // Last byte
    uint16_t nnn = op & 0xFFF;   // Last 12 bits

    // Select instruction's family
    switch (op >> 12) {
    case 0x0:
        if (op == 0x00E0) {
            // Clear display
            memset(c->display, 0, sizeof(c->display));
        } else {
            return CHIP8_ERR_BAD_OPCODE;
        }
        break;
    case 0x1:
        // Jump to address of last op 12-bits
        c->pc = nnn;
        break;
    case 0x6:
        // Set register X to last op byte
        c->v[x] = nn;
        break;
    case 0x7:
        // Add last op byte to register X (wraps at 256)
        c->v[x] += nn;
        break;
    case 0xA:
        // Set index to last op 12-bits;
        c->i = nnn;
        break;
    case 0xD: {
        // Wrap coordinates
        uint8_t px = c->v[x] % CHIP8_DISP_W;
        uint8_t py = c->v[y] % CHIP8_DISP_H;

        c->v[0xF] = 0; // Collision detection

        uint8_t sprite_byte;
        uint16_t idx;
        for (uint8_t r = 0; r < n; r++) {
            if (py + r >= CHIP8_DISP_H || c->i + r >= CHIP8_MEM_SIZE)
                break;
            sprite_byte = c->memory[c->i + r];
            for (uint8_t col = 0; col < 8; col++) {
                if (px + col >= CHIP8_DISP_W)
                    break;
                bool bit = (sprite_byte >> (7 - col)) & 1;
                idx = (px + col) + (py + r) * CHIP8_DISP_W;
                if (bit) {
                    if (c->display[idx]) {
                        c->v[0xF] = 1;
                    }
                    c->display[idx] ^= 1;
                }
            }
        }
        break;
    }
    default:
        // Unknown instruction
        return CHIP8_ERR_BAD_OPCODE;
    }
    return CHIP8_OK;
}