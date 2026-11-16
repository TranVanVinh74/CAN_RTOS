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
           $(wildcard $(NODE)/bsp/src/*.c)
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
  -I$(NODE)/bsp/inc \
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

-include $(NODE)/board_uid.mk

ST_FLASH   ?= st-flash
FLASH_ADDR ?= 0x08000000
UID_ADDR   := 0x1FFFF7E8
UID_BYTES  := 12
UID_FILE   := $(BUILD)/board_uid.bin

check-uid:
	@mkdir -p $(BUILD)
	@if ! $(ST_FLASH) read $(UID_FILE) $(UID_ADDR) $(UID_BYTES) >/dev/null 2>&1; then \
		echo "cannot read the board UID through ST-Link"; exit 1; \
	fi; \
	got=$$(xxd -p $(UID_FILE)); \
	if [ -z "$(BOARD_UID)" ]; then \
		echo "board UID $$got; $(NODE)/board_uid.mk is missing"; \
		echo "create it with: BOARD_UID := <UID of the $(TARGET) board>"; exit 1; \
	fi; \
	if [ "$$got" != "$(BOARD_UID)" ]; then \
		echo "UID mismatch: board $$got, $(TARGET) expects $(BOARD_UID)"; exit 1; \
	fi; \
	echo "board UID $$got matches $(TARGET)"

flash: $(BIN) check-uid
	$(ST_FLASH) --reset write $(BIN) $(FLASH_ADDR)

erase: check-uid
	$(ST_FLASH) erase

reset:
	$(ST_FLASH) reset

clean:
	rm -rf $(BUILD)

-include $(OBJ:.o=.d)

.PHONY: all clean check-uid flash erase reset
