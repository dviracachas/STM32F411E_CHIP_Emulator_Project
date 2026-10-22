# CHIP-8 Emulation Project for STM32F411E-Discovery board

[![Board](https://img.shields.io/badge/Board-STM32F411--DISCO-blue)](#)
[![Toolchain](https://img.shields.io/badge/Toolchain-arm--none--eabi--gcc-orange)](#)
[![Build](https://github.com/dviracachas/STM32F411E_CHIP_Emulator_Project/actions/workflows/build.yaml/badge.svg)](https://github.com/dviracachas/STM32F411E_CHIP_Emulator_Project/actions/workflows/build.yaml)

For-fun project to learn how to use the STM32F411E-Discovery board by making a CHIP-8 emulator, using CMSIS and direct compilation only.

# 🗺️ Roadmap
Current tasks and progress are reported in the related [GitHub Project](https://github.com/users/dviracachas/projects/3).

# 📁 Organization

This repo is organized as a monorepo that will include various projects sharing base libraries located in `vendor/` as submodules.
```text
.
├── linker/                   # Shared linker script
├── vendor/                   # Git submodules (CMSIS 6, CMSIS Device F4 and STM32 Cube F4)
└── projects/                 # Subprojects added over time
    └── Simple_LED_blink/     # Base output test
    └── Simple_button_LED/    # Base input/output test
```
# 📚️ Used technologies
- Toolchain: `arm-none-eabi-gcc` v16.2.0-1
- Build System: `make` v4.4.1-3
- Flasher: `st-flash` (stlink) v1.8.0-121

# 🛠️ Cloning and compiling a project
```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/dviracachas/STM32F411E_CHIP_Emulator_Project.git

# 2. Compile a project
cd projects/SELECTED_PROJECT/
make

# 3. Flash the board
make flash
```
