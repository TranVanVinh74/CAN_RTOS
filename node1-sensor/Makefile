# Standalone Makefile for node1-sensor (STM32F103C8T6) - arm-none-eabi-gcc, no CubeIDE.
# Usage: cd node1-sensor && make   (or: make -C node1-sensor)

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
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c \
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
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c \
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c

TARGET  := node1-sensor
LD_FILE := $(NODE)/STM32F103C8TX_FLASH.ld

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

ST_FLASH   ?= st-flash
FLASH_ADDR ?= 0x08000000

flash: $(BIN)
	$(ST_FLASH) --reset write $(BIN) $(FLASH_ADDR)

erase:
	$(ST_FLASH) erase

reset:
	$(ST_FLASH) reset

clean:
	rm -rf $(BUILD)

-include $(OBJ:.o=.d)

.PHONY: all clean flash erase reset
