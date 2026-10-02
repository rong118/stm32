TARGET = bluepill

BUILD = build

CC = arm-none-eabi-gcc
AS = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

CFLAGS = \
	-mcpu=cortex-m3 \
	-mthumb \
	-ffreestanding \
	-fdata-sections \
	-ffunction-sections \
	-Wall \
	-Wextra \
	-g3 \
	-O0

ASFLAGS = \
	-mcpu=cortex-m3 \
	-mthumb \
	-g3

LDFLAGS = \
	-T linker.ld \
	-nostdlib \
	-Wl,--gc-sections \
	-Wl,-Map=$(BUILD)/$(TARGET).map

C_SOURCES = \
	src/main.c

ASM_SOURCES = \
	startup/startup.s

OBJECTS = \
	$(BUILD)/main.o \
	$(BUILD)/startup.o

ELF = $(BUILD)/$(TARGET).elf
BIN = $(BUILD)/$(TARGET).bin

all: $(ELF) $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/main.o: src/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup.o: startup/startup.s | $(BUILD)
	$(AS) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

flash: $(ELF)
	openocd \
		-f interface/stlink.cfg \
		-f target/stm32f1x.cfg \
		-c "program $(ELF) verify reset exit"

debug: $(ELF)
	openocd \
		-f interface/stlink.cfg \
		-f target/stm32f1x.cfg

clean:
	rm -rf $(BUILD)

.PHONY: all flash debug clean
