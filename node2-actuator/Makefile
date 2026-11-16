# Standalone Makefile for node2-actuator (STM32F103C8T6) - arm-none-eabi-gcc, no CubeIDE.
# Usage: cd node2-actuator && make   (or: make -C node2-actuator)

NODE    := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
COMMON  := $(abspath $(NODE)/../common)
BUILD   := $(NODE)/build

TC ?= arm-none-eabi-
CC      := $(TC)gcc
OBJCOPY := $(TC)objcopy
SIZE    := $(TC)size

CPU  := -mcpu=cortex-m3
DEFS := -DSTM32F103xB -DUSE_HAL_DRIVER -DDEBUG
OPT  ?= -O0

CFLAGS  := $(CPU) -mthumb -std=gnu11 $(OPT) -g3 $(DEFS) -Wall \
           -ffunction-sections -fdata-sections -MMD -MP --specs=nano.specs
ASFLAGS := $(CPU) -mthumb -g3 $(DEFS) -x assembler-with-cpp -MMD -MP
LDFLAGS := $(CPU) -mthumb -mfloat-abi=soft \
           --specs=nosys.specs --specs=nano.specs \
           -Wl,--gc-sections -static \
           -Wl,--start-group -lc -lm -Wl,--end-group

CORE_SRC := \
  Core/Src/main.c \
  Core/Src/freertos.c \
  Core/Src/stm32f1xx_hal_msp.c \
  Core/Src/stm32f1xx_hal_timebase_tim.c \
  Core/Src/stm32f1xx_it.c \
  Core/Src/syscalls.c \
  Core/Src/sysmem.c \
  Core/Src/system_stm32f1xx.c

STARTUP_SRC := Core/Startup/startup_stm32f103c8tx.s

FREERTOS_SRC := \
  Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
  Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
  Middlewares/Third_Party/FreeRTOS/Source/list.c \
  Middlewares/Third_Party/FreeRTOS/Source/queue.c \
  Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
  Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
  Middlewares/Third_Party/FreeRTOS/Source/timers.c \
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c \
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c \
  Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c

HAL_SRC := \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_can.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c

TARGET  := node2-actuator
LD_FILE := $(NODE)/STM32F103C8TX_SETTINGS.ld

APP_SRC := $(wildcard $(NODE)/app/src/*.c) \
           $(wildcard $(NODE)/hal/src/*.c) \
           $(wildcard $(NODE)/bsp/*.c)
SRC := $(CORE_SRC) $(FREERTOS_SRC) $(HAL_SRC) $(patsubst $(NODE)/%,%,$(APP_SRC))
OBJ := $(addprefix $(BUILD)/, $(patsubst %.c,%.o,$(SRC)) $(patsubst %.s,%.o,$(STARTUP_SRC)))

ELF := $(BUILD)/$(TARGET).elf
HEX := $(BUILD)/$(TARGET).hex
BIN := $(BUILD)/$(TARGET).bin

INC := -I$(NODE)/Core/Inc \
  -I$(NODE)/Drivers/STM32F1xx_HAL_Driver/Inc \
  -I$(NODE)/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy \
  -I$(NODE)/Middlewares/Third_Party/FreeRTOS/Source/include \
  -I$(NODE)/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS \
  -I$(NODE)/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 \
  -I$(NODE)/Drivers/CMSIS/Device/ST/STM32F1xx/Include \
  -I$(NODE)/Drivers/CMSIS/Include \
  -I$(NODE)/bsp \
  -I$(NODE)/hal/inc \
  -I$(NODE)/app/inc \
  -I$(COMMON)

all: $(ELF) $(HEX) $(BIN)

$(BUILD)/%.o: $(NODE)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS) $(INC)

$(BUILD)/%.o: $(NODE)/%.s
	@mkdir -p $(dir $@)
	$(CC) -c $(ASFLAGS) $< -o $@ $(INC)

$(ELF): $(OBJ) $(LD_FILE)
	$(CC) -o $@ $(OBJ) $(LDFLAGS) -T"$(LD_FILE)" -Wl,-Map="$(BUILD)/$(TARGET).map"
	$(SIZE) $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

OPENOCD    ?= openocd
FLASH_ADDR ?= 0x08000000

# openocd erases only the sectors the image covers, so the settings page at
# 0x0800FC00 is preserved across 'make flash'.
flash: $(BIN)
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg \
		-c "init" -c "reset halt" \
		-c "flash write_image erase unlock $(BIN) $(FLASH_ADDR)" \
		-c "reset run" -c "shutdown"

# full-chip erase, including the settings page at 0x0800FC00
erase:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg \
		-c "init" -c "reset halt" \
		-c "flash erase_sector 0 0 63" \
		-c "shutdown"

reset:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg \
		-c "init" -c "reset run" -c "shutdown"

clean:
	rm -rf $(BUILD)

-include $(OBJ:.o=.d)

.PHONY: all clean flash erase reset
