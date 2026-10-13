TOP_DIR := ../..
TARGET  = build/firmware
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU     = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
INCS    = -I$(TOP_DIR)/vendor/cmsis_device_f4/Include \
		  -I$(TOP_DIR)/vendor/CMSIS_6/CMSIS/Core/Include
CFLAGS  = $(CPU) -O0 -g3 -Wall -Wextra -DSTM32F411xE $(INCS) \
          -ffunction-sections -fdata-sections
ASFLAGS = $(CPU) -g3
LDFLAGS = $(CPU) -T$(TOP_DIR)/linker/STM32F411VETX_FLASH.ld -Wl,-Map=$(TARGET).map \
          -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs

SRCS = src/main.c $(TOP_DIR)/vendor/cmsis_device_f4/Source/Templates/system_stm32f4xx.c
ASMS = $(TOP_DIR)/vendor/cmsis_device_f4/Source/Templates/gcc/startup_stm32f411xe.s
OBJS = $(SRCS:%.c=build/%.o) $(ASMS:%.s=build/%.o)

all: $(TARGET).bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

flash: $(TARGET).bin
	st-flash write $(TARGET).bin 0x8000000

clean:
	rm -rf build

.PHONY: all flash clean