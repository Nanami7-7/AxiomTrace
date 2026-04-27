# =============================================================================
# MSPM0G3507 Project Makefile
# Five-layer architecture: APP -> OSAL + BSP -> HAL -> FreeRTOS + SDK
# =============================================================================

# ---------- Toolchain ----------
PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
AS      := $(PREFIX)gcc
LD      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

# ---------- Project ----------
PROJECT := msmp0g3507_demo
OUTDIR  := build

# ---------- MCU Parameters ----------
MCU      := cortex-m0plus
CPUFLAGS := -mcpu=$(MCU) -mthumb
FPUFLAGS :=

# ---------- Compiler Flags ----------
CFLAGS  := $(CPUFLAGS) $(FPUFLAGS)
CFLAGS  += -std=c11 -Wall -Wextra -Wshadow -Wredundant-decls
CFLAGS  += -ffunction-sections -fdata-sections -fno-common
CFLAGS  += -Os -g3
CFLAGS  += -DMSM0G3507 -D__STARTUP_CLEAR_BSS

# ---------- Include Paths ----------
INCDIRS := \
	-Iapp/inc \
	-Iosal/inc \
	-Ibsp/inc \
	-Ihal/inc \
	-Ifreertos/Source/include \
	-Ifreertos/Source/portable/GCC/ARM_CM0 \
	-Ifreertos \
	-Isdk/inc \
	-Icmsis/inc \
	-Istartup

CFLAGS += $(INCDIRS)

# ---------- Assembler Flags ----------
ASFLAGS := $(CPUFLAGS) $(INCDIRS) -D__ASSEMBLY__

# ---------- Linker Flags ----------
LDSCRIPT := linker/mspm0g3507.ld
LDFLAGS  := $(CPUFLAGS) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(OUTDIR)/$(PROJECT).map
LDFLAGS  += --specs=nano.specs --specs=nosys.specs
LDFLAGS  += -Wl,--start-group -lc -lm -Wl,--end-group

# ---------- Source Files ----------
# APP
APP_SRCS := \
	app/src/app_main.c \
	app/src/app_task.c

# OSAL
OSAL_SRCS := \
	osal/src/osal_task.c \
	osal/src/osal_delay.c \
	osal/src/osal_semaphore.c \
	osal/src/osal_mutex.c \
	osal/src/osal_queue.c \
	osal/src/osal_timer.c \
	osal/src/osal_critical.c

# BSP
BSP_SRCS := \
	bsp/src/bsp_board.c \
	bsp/src/bsp_gpio.c \
	bsp/src/bsp_uart.c \
	bsp/src/bsp_spi.c \
	bsp/src/bsp_i2c.c \
	bsp/src/bsp_adc.c \
	bsp/src/bsp_motor.c \
	bsp/src/bsp_servo.c \
	bsp/src/bsp_mpu6050.c \
	bsp/src/bsp_oled.c \
	bsp/src/bsp_tracer.c \
	bsp/src/bsp_hcsr04.c

# HAL
HAL_SRCS := \
	hal/src/hal_gpio.c \
	hal/src/hal_uart.c \
	hal/src/hal_spi.c \
	hal/src/hal_i2c.c \
	hal/src/hal_adc.c \
	hal/src/hal_timer.c \
	hal/src/hal_pwm.c \
	hal/src/hal_dac.c

# FreeRTOS
FREERTOS_SRCS := \
	freertos/Source/tasks.c \
	freertos/Source/queue.c \
	freertos/Source/list.c \
	freertos/Source/timers.c \
	freertos/Source/portable/GCC/ARM_CM0/port.c \
	freertos/Source/portable/MemMang/heap_4.c

# Startup & CMSIS
STARTUP_SRCS := \
	startup/startup_mspm0g3507.c \
	cmsis/src/system_mspm0g3507.c

# All sources
SRCS := $(APP_SRCS) $(OSAL_SRCS) $(BSP_SRCS) $(HAL_SRCS) $(FREERTOS_SRCS) $(STARTUP_SRCS)

# Object files
OBJS := $(patsubst %.c,$(OUTDIR)/%.o,$(SRCS))

# ---------- Output ----------
ELF   := $(OUTDIR)/$(PROJECT).elf
BIN   := $(OUTDIR)/$(PROJECT).bin
HEX   := $(OUTDIR)/$(PROJECT).hex

# ========== Rules ==========

.PHONY: all clean flash size

all: $(ELF) $(BIN) $(HEX) size

$(ELF): $(OBJS) | $(OUTDIR)
	$(LD) $(LDFLAGS) -o $@ $^

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(OUTDIR)/%.o: %.c | $(OUTDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR):
	mkdir -p $(OUTDIR)

size: $(ELF)
	$(SIZE) --format=berkeley $(ELF)

clean:
	rm -rf $(OUTDIR)

flash: $(HEX)
	openocd -f interface/cmsis-dap.cfg -f target/ti_mspm0.cfg -c "program $(HEX) verify reset exit"
