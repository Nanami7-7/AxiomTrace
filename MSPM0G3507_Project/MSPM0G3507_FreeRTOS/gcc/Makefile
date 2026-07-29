PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
BUILD_DIR := $(PROJECT_ROOT)/gcc/build
GCC_BIN ?= D:/GD32EmbeddedBuilder_v1.5.6_Rel/GD32EmbeddedBuilder_v1.5.6_Rel/Tools/GNU Tools ARM Embedded/xpack-arm-none-eabi-gcc/9.2.1-1.1/bin
CC := "$(GCC_BIN)/arm-none-eabi-gcc.exe"
AS := "$(GCC_BIN)/arm-none-eabi-gcc.exe"
OBJCOPY := "$(GCC_BIN)/arm-none-eabi-objcopy.exe"
SIZE := "$(GCC_BIN)/arm-none-eabi-size.exe"
TARGET := $(BUILD_DIR)/MSPM0G3507_FreeRTOS
ELF := $(TARGET).elf
BIN := $(TARGET).bin
HEX := $(TARGET).hex
MAP := $(TARGET).map
INCLUDES := \
  -I$(PROJECT_ROOT)/Config -I$(PROJECT_ROOT) \
  -I$(PROJECT_ROOT)/Application -I$(PROJECT_ROOT)/Application/test \
  -I$(PROJECT_ROOT)/Application/Task -I$(PROJECT_ROOT)/Application/Algorithm \
  -I$(PROJECT_ROOT)/Application/Algorithm/Filter -I$(PROJECT_ROOT)/BSP \
  -I$(PROJECT_ROOT)/BSP/Input -I$(PROJECT_ROOT)/BSP/Peripherals \
  -I$(PROJECT_ROOT)/BSP/IMU -I$(PROJECT_ROOT)/BSP/IMU/Ports/mspm0g3507 \
  -I$(PROJECT_ROOT)/Lib/OSAL -I$(PROJECT_ROOT)/Lib/Math \
  -I$(PROJECT_ROOT)/Lib/AxiomTrace -I$(PROJECT_ROOT)/Lib/AxiomTrace/Ports/mspm0 \
  -I$(PROJECT_ROOT)/Lib/FreeRTOS/include -I$(PROJECT_ROOT)/Lib/FreeRTOS/src \
  -I$(PROJECT_ROOT)/Lib/FreeRTOS/portable/MemMang \
  -I$(PROJECT_ROOT)/Lib/FreeRTOS/portable/GCC/ARM_CM0 \
  -I$(PROJECT_ROOT)/../source -I$(PROJECT_ROOT)/../source/third_party/CMSIS/Core/Include
DEFINES := \
  -D__MSPM0G3507__ -D_KEY_T_DECLARED -DLOG_LEVEL=LOG_LEVEL_INFO \
  -DPRJ_FILTER_ENABLE_COMPLEMENTARY=0 -DPRJ_FILTER_ENABLE_LPF=0 \
  -DPRJ_FILTER_ENABLE_EKF=0 -DPRJ_FILTER_ENABLE_LKF=0 \
  -DPRJ_FILTER_ENABLE_MAHONY=0 -DPRJ_FILTER_ENABLE_MADGWICK=0 \
  -DPRJ_FILTER_ENABLE_KF=1
CPUFLAGS := -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -mno-unaligned-access
CFLAGS := $(CPUFLAGS) $(INCLUDES) $(DEFINES) -Wall -Wextra -Wno-unused-parameter -std=c99 -Os -g3 -ffunction-sections -fdata-sections -fno-common
ASFLAGS := $(CPUFLAGS) $(INCLUDES) -x assembler-with-cpp -g3
LDFLAGS := $(CPUFLAGS) -T$(PROJECT_ROOT)/gcc/mspm0g3507_gcc.ld -Wl,-Map,$(MAP) -Wl,--gc-sections -Wl,--print-memory-usage -Wl,--cref -nostartfiles --specs=nano.specs --specs=nosys.specs
LDLIBS := -L$(PROJECT_ROOT)/../source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x -Wl,--start-group -l:driverlib.a -lc -lm -lgcc -Wl,--end-group
SOURCES := $(PROJECT_ROOT)/main.c $(wildcard $(PROJECT_ROOT)/Config/*.c) $(wildcard $(PROJECT_ROOT)/Application/*.c) $(wildcard $(PROJECT_ROOT)/Application/Algorithm/*.c) $(wildcard $(PROJECT_ROOT)/Application/Algorithm/Filter/*.c) $(wildcard $(PROJECT_ROOT)/Application/Task/*.c) $(wildcard $(PROJECT_ROOT)/Application/test/*.c) $(wildcard $(PROJECT_ROOT)/BSP/*.c) $(wildcard $(PROJECT_ROOT)/BSP/Input/*.c) $(wildcard $(PROJECT_ROOT)/BSP/Peripherals/*.c) $(wildcard $(PROJECT_ROOT)/BSP/IMU/*.c) $(wildcard $(PROJECT_ROOT)/BSP/IMU/Ports/mspm0g3507/*.c) $(wildcard $(PROJECT_ROOT)/Lib/OSAL/*.c) $(wildcard $(PROJECT_ROOT)/Lib/Math/*.c) $(wildcard $(PROJECT_ROOT)/Lib/AxiomTrace/Ports/mspm0/*.c) $(PROJECT_ROOT)/Lib/FreeRTOS/src/list.c $(PROJECT_ROOT)/Lib/FreeRTOS/src/queue.c $(PROJECT_ROOT)/Lib/FreeRTOS/src/tasks.c $(PROJECT_ROOT)/Lib/FreeRTOS/src/timers.c $(PROJECT_ROOT)/Lib/FreeRTOS/portable/MemMang/heap_4.c $(PROJECT_ROOT)/Lib/FreeRTOS/portable/GCC/ARM_CM0/port.c
OBJECTS := $(patsubst $(PROJECT_ROOT)/%.c,$(BUILD_DIR)/%.o,$(SOURCES)) $(BUILD_DIR)/gcc/startup_mspm0g350x_gcc.o
.PHONY: all clean size probe flash
all: $(ELF) $(BIN) $(HEX) size
$(ELF): $(OBJECTS) $(PROJECT_ROOT)/gcc/mspm0g3507_gcc.ld
	@echo Linking $@
	@$(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@
$(BIN): $(ELF)
	@$(OBJCOPY) -O binary $< $@
$(HEX): $(ELF)
	@$(OBJCOPY) -O ihex $< $@
$(BUILD_DIR)/%.o: $(PROJECT_ROOT)/%.c
	@mkdir -p "$(dir $@)"
	@echo Compiling $<
	@$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(BUILD_DIR)/gcc/startup_mspm0g350x_gcc.o: $(PROJECT_ROOT)/gcc/startup_mspm0g350x_gcc.s
	@mkdir -p "$(dir $@)"
	@echo Assembling $<
	@$(AS) $(ASFLAGS) -c $< -o $@
-include $(OBJECTS:.o=.d)
size: $(ELF)
	@$(SIZE) $(ELF)
clean:
	@rm -rf "$(BUILD_DIR)"
probe:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(PROJECT_ROOT)/gcc/download_jlink.ps1" -ProbeOnly
flash: $(HEX)
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(PROJECT_ROOT)/gcc/download_jlink.ps1" -Program
