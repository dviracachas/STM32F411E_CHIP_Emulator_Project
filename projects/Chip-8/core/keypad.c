/* core/keypad.c */
#include "keypad.h"

static const uint8_t pos_to_key[16] = {0x1, 0x2, 0x3, 0xC, 0x4, 0x5, 0x6, 0xD,
                                       0x7, 0x8, 0x9, 0xE, 0xA, 0x0, 0xB, 0xF};

void keypad_init(Keypad *k)
{
    k->candidate = 0;
    k->stable = 0;
    k->count = 0;
}

bool keypad_update(Keypad *k, uint16_t pos_mask)
{
    if (pos_mask != k->candidate) {
        k->candidate = pos_mask;
        k->count = 1;
    } else if (k->count < 255) {
        k->count++;
    }

    if (k->count >= KEYPAD_DEBOUNCE_SCANS && k->stable != k->candidate) {
        k->stable = k->candidate;
        return true;
    }
    return false;
}

uint16_t keypad_pressed(const Keypad *k)
{
    return k->stable;
}

uint16_t keypad_to_chip8(uint16_t pos_mask)
{
    uint16_t out = 0;
    for (unsigned p = 0; p < 16; p++) {
        if (pos_mask & (1U << p)) {
            out |= (uint16_t)(1U << pos_to_key[p]);
        }
    }
    return out;
}