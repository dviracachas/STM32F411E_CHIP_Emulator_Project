#ifndef ROMS_H
#define ROMS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t len;
} rom_entry_t;

size_t rom_count(void);
const rom_entry_t *rom_get(size_t index);

extern const rom_entry_t user_roms[];
extern const unsigned user_rom_count;

#endif