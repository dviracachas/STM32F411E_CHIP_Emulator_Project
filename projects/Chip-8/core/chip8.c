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
    c->rng_state = 0xDD7B6B3F;                               // Initialize random state
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

static uint8_t next_random(Chip8 *c)
{
    uint32_t s = c->rng_state;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    c->rng_state = s;
    return (uint8_t)(s >> 8);
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
        switch (op) {
        case 0x00E0:
            // 00E0: Clear display
            memset(c->display, 0, sizeof(c->display));
            break;
        case 0x00EE:
            // 00EE: Pop the stack into pc
            if (c->sp == 0) {
                return CHIP8_ERR_STACK;
            }
            c->sp--;
            c->pc = c->stack[c->sp];
            break;
        default:
            return CHIP8_ERR_BAD_OPCODE;
        }
        break;
    case 0x1:
        // 1NNN: Jump to address of last op 12-bits
        c->pc = nnn;
        break;
    case 0x2:
        // 2NNN: Push pc into stack
        if (c->sp == 16) {
            return CHIP8_ERR_STACK;
        }
        c->stack[c->sp] = c->pc;
        c->sp++;
        c->pc = nnn;
        break;
    case 0x3:
        // 3XNN: Skip when v[x] == nn
        if (c->v[x] == nn)
            c->pc += 2;
        break;
    case 0x4:
        // 4XNN: Skip when v[x] != nn
        if (c->v[x] != nn)
            c->pc += 2;
        break;
    case 0x5:
        if (n == 0) {
            // 5XY0: Skip when v[x] == v[y]
            if (c->v[x] == c->v[y])
                c->pc += 2;
        } else {
            return CHIP8_ERR_BAD_OPCODE;
        }
        break;
    case 0x6:
        // 6XNN: Set register X to last op byte
        c->v[x] = nn;
        break;
    case 0x7:
        // 7XNN: Add last op byte to register X (wraps at 256)
        c->v[x] += nn;
        break;
    case 0x8:
        switch (n) {
        case 0x0: // 8XY0
            c->v[x] = c->v[y];
            break;
        case 0x1: // 8XY1
            c->v[x] |= c->v[y];
            break;
        case 0x2: // 8XY2
            c->v[x] &= c->v[y];
            break;
        case 0x3: // 8XY3
            c->v[x] ^= c->v[y];
            break;
        case 0x4: { // 8XY4
            uint16_t sum = c->v[x] + c->v[y];
            uint8_t overflow = (sum > 0xFF);
            c->v[x] = (uint8_t)sum;
            c->v[0xF] = overflow;
            break;
        }
        case 0x5: { // 8XY5
            uint8_t no_borrow = (c->v[x] >= c->v[y]);
            c->v[x] -= c->v[y];
            c->v[0xF] = no_borrow;
            break;
        }
        case 0x6: { // 8XY6
            bool moved_bit = c->v[x] & 0x1;
            c->v[x] = c->v[x] >> 1;
            c->v[0xF] = moved_bit;
            break;
        }
        case 0x7: { // 8XY7
            uint8_t no_borrow = (c->v[y] >= c->v[x]);
            c->v[x] = c->v[y] - c->v[x];
            c->v[0xF] = no_borrow;
            break;
        }
        case 0xE: { // 8XYE
            bool moved_bit = c->v[x] & 0x80;
            c->v[x] = c->v[x] << 1;
            c->v[0xF] = moved_bit;
            break;
        }
        default:
            return CHIP8_ERR_BAD_OPCODE;
        }
        break;
    case 0x9:
        if (n == 0) {
            // 9XY0: Skip when v[x] != v[y]
            if (c->v[x] != c->v[y])
                c->pc += 2;
        } else {
            return CHIP8_ERR_BAD_OPCODE;
        }
        break;
    case 0xA:
        // ANNN: Set index to last op 12-bits;
        c->i = nnn;
        break;
    case 0xB:
        // BNNN
        c->pc = nnn + c->v[0];
        break;
    case 0xC:
        // CXNN
        c->v[x] = next_random(c) & nn;
        break;
    case 0xD: {
        // DXYN: Draw a Nx8 sprite at X,Y
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
    case 0xF:
        switch (nn) {
        case 0x1E:
            // FX1E
            if (c->i + c->v[x] >= CHIP8_MEM_SIZE)
                return CHIP8_ERR_MEMORY;
            c->i += c->v[x];
            break;
        case 0x29:
            // FX29
            c->i = CHIP8_FONT_START + (c->v[x] & 0xF) * 5;
            break;
        case 0x33: {
            // FX33
            if ((uint16_t)(c->i + 2) >= CHIP8_MEM_SIZE)
                return CHIP8_ERR_MEMORY;
            uint8_t hundreds = c->v[x] / 100;
            uint8_t tens = (c->v[x] / 10) % 10;
            uint8_t units = c->v[x] % 10;
            c->memory[c->i] = hundreds;
            c->memory[c->i + 1] = tens;
            c->memory[c->i + 2] = units;
            break;
        }
        case 0x55:
            // FX55
            if (c->i + x >= CHIP8_MEM_SIZE)
                return CHIP8_ERR_MEMORY;
            for (uint8_t idx = 0; idx <= x; idx++) {
                c->memory[c->i + idx] = c->v[idx];
            }
            break;
        case 0x65:
            // FX65
            if (c->i + x >= CHIP8_MEM_SIZE)
                return CHIP8_ERR_MEMORY;
            for (uint8_t idx = 0; idx <= x; idx++) {
                c->v[idx] = c->memory[c->i + idx];
            }
            break;
        default:
            return CHIP8_ERR_BAD_OPCODE;
        }
        break;
    default:
        // Unknown instruction
        return CHIP8_ERR_BAD_OPCODE;
    }
    return CHIP8_OK;
}