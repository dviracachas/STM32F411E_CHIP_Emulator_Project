TOP_DIR := ../..
CORE_DIR := ../Chip-8/core
TARGET  = build/$(notdir $(CURDIR))
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU     = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
INCS    = -I$(TOP_DIR)/vendor/cmsis_device_f4/Include \
		  -I$(TOP_DIR)/vendor/CMSIS_6/CMSIS/Core/Include \
		  -I$(CORE_DIR) \
		  -Isrc
CFLAGS  = $(CPU) -O0 -g3 -Wall -Wextra -DSTM32F411xE $(INCS) \
          -ffunction-sections -fdata-sections
ASFLAGS = $(CPU) -g3
LDFLAGS = $(CPU) -T$(TOP_DIR)/linker/STM32F411VETX_FLASH.ld -Wl,-Map=$(TARGET).map \
          -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs

SRCS = src/main.c src/roms_builtin.c $(TOP_DIR)/vendor/cmsis_device_f4/Source/Templates/system_stm32f4xx.c
CORE_SRCS = $(CORE_DIR)/chip8.c $(CORE_DIR)/keypad.c
ASMS = $(TOP_DIR)/vendor/cmsis_device_f4/Source/Templates/gcc/startup_stm32f411xe.s
# Maps TOP_DIR to vendor/ so that it builds on build/vendor and build/src
OBJS_C = $(patsubst $(TOP_DIR)/%, build/%, $(filter $(TOP_DIR)/%, $(SRCS))) \
         $(patsubst %, build/%, $(filter-out $(TOP_DIR)/%, $(SRCS)))
OBJS_S = $(patsubst $(TOP_DIR)/%, build/%, $(filter $(TOP_DIR)/%, $(ASMS))) \
         $(patsubst %, build/%, $(filter-out $(TOP_DIR)/%, $(ASMS)))
OBJS_CORE = $(patsubst $(CORE_DIR)/%.c, build/core/%.o, $(CORE_SRCS))
OBJS = $(OBJS_C:.c=.o) $(OBJS_S:.s=.o) $(OBJS_CORE) build/gen/user_roms.o

USER_ROMS := $(wildcard roms/*.ch8)

all: $(TARGET).bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Rule for compiling from external sources (vendor/...)
build/vendor/%.o: $(TOP_DIR)/vendor/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/vendor/%.o: $(TOP_DIR)/vendor/%.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# Rule for compiling the CHIP-8 core
build/core/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling from local sources (src/...)
build/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/src/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

build/gen/user_roms.c: tools/gen_roms.sh $(USER_ROMS)
	@mkdir -p $(dir $@)
	bash tools/gen_roms.sh $@ $(USER_ROMS)

build/gen/%.o: build/gen/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

flash: $(TARGET).bin
	st-flash write $(TARGET).bin 0x8000000

clean:
	rm -rf build

.PHONY: all flash clean