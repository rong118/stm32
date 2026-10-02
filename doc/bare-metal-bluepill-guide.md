# Bare-Metal STM32F103C8T6 (Blue Pill) Blinky Project

A complete walkthrough for building a bare-metal LED blink project on the Blue Pill board using only command-line tools. No IDE, no HAL library, no CMSIS — just direct register manipulation and a minimal toolchain.

---

## Goal

```
PC --> ARM GCC --> ELF/BIN --> OpenOCD --> ST-LINK --> Blue Pill --> PC13 LED blinks
```

This is intended as a foundation that can later be extended to UART, Timer, Interrupt, DMA, and FreeRTOS.

---

## Project Structure

```
bluepill/
├── Makefile           # Build system: compile, link, flash, debug, clean
├── linker.ld          # Memory layout: where Flash and RAM live, section placement
├── src/
│   └── main.c         # Application code: register-level LED blink
├── startup/
│   └── startup.s      # ARM assembly: vector table, Reset_Handler, .data/.bss init
└── build/             # Output directory: .elf, .bin, .map, .o files
```

---

## Hardware Setup

### Required Components

- **STM32F103C8T6 Blue Pill** board
- **ST-LINK/V2** programmer/debugger
- USB data cable
- Linux or WSL on the host PC

### Wiring (SWD Connection)

```
ST-LINK        Blue Pill
─────────────────────────
SWDIO    -->   SWDIO (PA13)
SWCLK    -->   SWCLK (PA14)
GND      -->   GND
3.3V     -->   3.3V
```

The onboard LED is connected to **PC13** (active low on most Blue Pill boards).

---

## Toolchain Installation (Ubuntu / WSL)

```bash
sudo apt update
sudo apt install \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    gdb-multiarch \
    openocd \
    make
```

### Verify Installation

```bash
arm-none-eabi-gcc --version
arm-none-eabi-objcopy --version
gdb-multiarch --version
openocd --version
make --version
```

### What Each Tool Does

| Tool | Purpose |
|------|---------|
| `arm-none-eabi-gcc` | Cross-compiler: compiles C to ARM Cortex-M3 machine code |
| `arm-none-eabi-objcopy` | Converts ELF to raw binary for flashing |
| `gdb-multiarch` | Debugger that can target ARM from an x86 host |
| `openocd` | Bridges GDB/flash commands to the ST-LINK hardware |
| `make` | Build automation |

---

## File-by-File Breakdown

### 1. `src/main.c` — Application Code

```c
#include <stdint.h>

#define RCC_BASE        0x40021000UL
#define GPIOC_BASE      0x40011000UL

#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))

#define RCC_IOPCEN      (1U << 4)
#define GPIO_PIN_13     (1U << 13)

static void delay(void)
{
    for (volatile uint32_t i = 0; i < 500000; i++)
    {
        __asm volatile ("nop");
    }
}

int main(void)
{
    RCC_APB2ENR |= RCC_IOPCEN;

    GPIOC_CRH &= ~(0xFU << 20);
    GPIOC_CRH |=  (0x2U << 20);

    while (1)
    {
        GPIOC_ODR ^= GPIO_PIN_13;
        delay();
    }
}
```

#### Key Concepts

**Register addresses** are memory-mapped. The STM32F103 datasheet defines:

```
0x40021000  -->  RCC (Reset & Clock Control)
                  └── offset 0x18: APB2ENR (peripheral clock enable register)
                       └── bit 4: IOPCEN (GPIOC clock enable)

0x40011000  -->  GPIOC
                  ├── offset 0x04: CRH (config register for pins 8-15)
                  │    └── bits 23:20 control PC13
                  │         MODE13 = 0b10 (output, 2 MHz)
                  │         CNF13  = 0b00 (push-pull)
                  └── offset 0x0C: ODR (output data register)
                       └── bit 13: PC13 value (XOR to toggle)
```

**Why no HAL?** This approach forces you to understand what the hardware is actually doing. Every HAL call maps to register writes like these under the hood.

**Why `volatile`?** These pointers target hardware registers that can change outside the compiler's knowledge. Without `volatile`, the compiler might optimize away reads/writes.

---

### 2. `startup/startup.s` — Startup Assembly

```asm
.syntax unified
.cpu cortex-m3
.thumb

.global Reset_Handler
.global Default_Handler
.extern main

.section .isr_vector, "a", %progbits
g_pfnVectors:
    .word _estack          @ Initial stack pointer
    .word Reset_Handler    @ Reset vector (entry point)
    .word Default_Handler  @ NMI
    .word Default_Handler  @ HardFault
    .word Default_Handler  @ MemManage
    .word Default_Handler  @ BusFault
    .word Default_Handler  @ UsageFault
    .word 0                @ Reserved
    .word 0                @ Reserved
    .word 0                @ Reserved
    .word Default_Handler  @ SVCall
    .word Default_Handler  @ Debug Monitor
    .word 0                @ Reserved
    .word Default_Handler  @ PendSV
    .word Default_Handler  @ SysTick
    .word Default_Handler  @ IRQ0 (first external interrupt)
```

The `Reset_Handler` does three things before calling `main()`:

```asm
Reset_Handler:
    @ 1. Copy .data section from Flash to RAM
    ldr r0, =_sidata       @ source: .data initial values in Flash
    ldr r1, =_sdata        @ dest start: .data in RAM
    ldr r2, =_edata        @ dest end
    @ ... word-by-word copy loop ...

    @ 2. Zero out .bss section in RAM
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
    @ ... word-by-word zero loop ...

    @ 3. Call main
    bl main

hang:
    b hang                 @ infinite loop if main() ever returns
```

#### Boot Sequence

```
Power On
   |
   v
CPU reads address 0x08000000     --> gets initial stack pointer (_estack)
CPU reads address 0x08000004     --> gets Reset_Handler address
   |
   v
Reset_Handler executes:
   |
   ├── Copy .data from Flash to RAM  (initialized globals)
   ├── Zero .bss in RAM              (uninitialized globals)
   |
   v
main()
   |
   v
LED blinks forever
```

**Why copy .data?** Global variables with initial values (e.g., `int x = 42;`) are stored in Flash at compile time but need to live in RAM at runtime so they can be modified. The startup code copies them over.

**Why zero .bss?** The C standard guarantees uninitialized globals start at zero. The startup code enforces this.

---

### 3. `linker.ld` — Linker Script

```ld
ENTRY(Reset_Handler)
_estack = ORIGIN(RAM) + LENGTH(RAM);

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
```

#### Memory Map

```
Flash (64 KB)                          RAM (20 KB)
0x08000000                             0x20000000
┌──────────────────┐                   ┌──────────────────┐
│ .isr_vector      │                   │ .data            │
│ (vector table)   │                   │ (copied from     │
├──────────────────┤                   │  Flash at boot)  │
│ .text            │                   ├──────────────────┤
│ (program code)   │                   │ .bss             │
├──────────────────┤                   │ (zeroed at boot) │
│ .rodata          │                   ├──────────────────┤
│ (const data)     │                   │                  │
├──────────────────┤                   │ (heap grows up)  │
│ .data initial    │  ── startup ──>   │                  │
│ values           │     copies to     │                  │
└──────────────────┘     RAM           │ (stack grows     │
                                       │  down from top)  │
                                       ├──────────────────┤
                                       │ _estack          │
                                       └──────────────────┘
                                       0x20005000
```

#### Section Details

| Section | Location | Description |
|---------|----------|-------------|
| `.isr_vector` | Flash | Vector table — must be at 0x08000000 |
| `.text` | Flash | Compiled machine code |
| `.rodata` | Flash | Read-only constants, string literals |
| `.data` | Flash (load), RAM (runtime) | Initialized global/static variables |
| `.bss` | RAM | Uninitialized global/static variables (zeroed) |
| Stack | RAM (top, grows down) | Function call frames, local variables |

The linker script exports symbols (`_sdata`, `_edata`, `_sbss`, `_ebss`, `_sidata`) that the startup assembly code uses to know where to copy/zero.

---

### 4. `Makefile` — Build System

#### Key Compiler Flags

| Flag | Purpose |
|------|---------|
| `-mcpu=cortex-m3` | Target the Cortex-M3 CPU in the STM32F103 |
| `-mthumb` | Use Thumb instruction set (required for Cortex-M) |
| `-ffreestanding` | No standard library startup or assumptions |
| `-fdata-sections -ffunction-sections` | Put each function/variable in its own section |
| `-Wl,--gc-sections` | Linker garbage-collects unused sections (smaller binary) |
| `-nostdlib` | Don't link the standard C library |
| `-g3 -O0` | Full debug info, no optimization (good for learning/debugging) |
| `-T linker.ld` | Use our custom linker script |

#### Make Targets

```bash
make          # Build: compile .c and .s, link to .elf, convert to .bin
make flash    # Flash the .elf to the board via OpenOCD + ST-LINK
make debug    # Start OpenOCD server for GDB debugging
make clean    # Remove all build artifacts
```

---

## Build and Verify

```bash
make
```

Expected output:

```
arm-none-eabi-size build/bluepill.elf

   text    data     bss     dec     hex
    300       0       0     300     12c
```

This means 300 bytes of code in Flash, no initialized data, no BSS — the entire program fits in 300 bytes.

### Build Artifacts

| File | Purpose |
|------|---------|
| `bluepill.elf` | Full binary with debug symbols — used by GDB |
| `bluepill.bin` | Raw binary image — used for flashing |
| `bluepill.map` | Linker map file — shows where every section and symbol ended up, how much Flash/RAM is used |

---

## Flash and Run

```bash
make flash
```

This runs OpenOCD which connects to the ST-LINK, programs the ELF into Flash, verifies the write, and resets the MCU. The LED on PC13 should start blinking.

## Debug with GDB

Terminal 1 — start OpenOCD server:

```bash
make debug
```

Terminal 2 — connect GDB:

```bash
gdb-multiarch build/bluepill.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) break main
(gdb) continue
```

---

## Summary of the Full Pipeline

```
main.c + startup.s
       |
       | arm-none-eabi-gcc (compile)
       v
    main.o + startup.o
       |
       | arm-none-eabi-gcc + linker.ld (link)
       v
   bluepill.elf (debug symbols + code)
       |
       ├── arm-none-eabi-objcopy --> bluepill.bin (raw binary)
       |
       ├── OpenOCD + ST-LINK --> Flash to MCU at 0x08000000
       |
       └── gdb-multiarch --> Step through code on real hardware
```

## What to Learn Next

This project is intentionally minimal. The natural progression from here:

1. **UART** — serial communication with the PC
2. **Timers** — hardware-based timing instead of busy-wait delay
3. **Interrupts (NVIC)** — event-driven programming, fill out the vector table
4. **DMA** — data transfer without CPU intervention
5. **FreeRTOS** — real-time operating system on top of this foundation
