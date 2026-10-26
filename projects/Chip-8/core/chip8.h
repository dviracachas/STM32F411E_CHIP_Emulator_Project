#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHIP8_MEM_SIZE 4096U
#define CHIP8_PROG_START 0x200U
#define CHIP8_FONT_START 0x050U
#define CHIP8_DISP_W 64U
#define CHIP8_DISP_H 32U

typedef struct {
    uint8_t memory[CHIP8_MEM_SIZE];
    uint8_t v[16]; // Registers V0..VF
    uint16_t i;    // Index register
    uint16_t pc;   // Program counter
    uint16_t stack[16];
    uint8_t sp; // Stack pointer (index)
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t display[CHIP8_DISP_W * CHIP8_DISP_H]; // 0 or 1 per pixel
    bool keys[16];
} Chip8;

typedef enum { CHIP8_OK = 0, CHIP8_ERR_TOO_BIG, CHIP8_ERR_BAD_OPCODE } Chip8Status;

void chip8_init(Chip8 *c); // Reset everything, load font
void chip8_set_key(Chip8 *c, uint8_t key, bool pressed);
Chip8Status chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len);
Chip8Status chip8_step(Chip8 *c); // Run an op (CHIP-8 instruction)

#endif