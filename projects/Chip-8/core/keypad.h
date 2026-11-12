#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdbool.h>
#include <stdint.h>

#define KEYPAD_DEBOUNCE_SCANS 3

typedef struct {
    uint16_t candidate;
    uint16_t stable;
    uint8_t count;
} Keypad;

void keypad_init(Keypad *k);
bool keypad_update(Keypad *k, uint16_t pos_mask);
uint16_t keypad_pressed(const Keypad *k);
uint16_t keypad_to_chip8(uint16_t pos_mask);

#endif