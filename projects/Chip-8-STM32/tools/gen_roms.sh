#!/usr/bin/env bash
# tools/gen_roms.sh  usage: gen_roms.sh out.c file1.ch8 file2.ch8 ...
out="$1"; shift
{
  echo '#include "roms.h"'
  i=0; names=()
  for f in "$@"; do
    echo "static const uint8_t rom_$i[] = {"
    xxd -i < "$f"
    echo "};"
    names+=("{ \"$(basename "$f" .ch8)\", rom_$i, sizeof rom_$i }")
    i=$((i+1))
  done
  echo "const rom_entry_t user_roms[] = {"
  for n in "${names[@]}"; do echo "  $n,"; done
  echo "  { 0, 0, 0 }"
  echo "};"
  echo "const unsigned user_rom_count = $i;"
} > "$out"
