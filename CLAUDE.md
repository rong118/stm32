# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal STM32F103C8T6 (Blue Pill) LED blinky. No HAL, no CMSIS — direct register manipulation. Targets Cortex-M3 via ARM GCC cross-compiler. The on-board LED is on **PC13, active-low** (write 0 = LED on).

`doc/bare-metal-bluepill-guide.md` is the long-form walkthrough (SWD wiring, toolchain setup, file-by-file explanation, GDB session). Keep it in sync when changing the code it quotes.

## Build Commands

```bash
make          # Build ELF and BIN; prints section sizes via arm-none-eabi-size
make flash    # Flash via OpenOCD + ST-LINK (program, verify, reset)
make debug    # Start OpenOCD server; then: gdb-multiarch build/bluepill.elf -> target remote :3333
make clean    # Remove build/
```

There are no tests or linters. "Verification" means a clean build with `-Wall -Wextra`, checking the size output fits 64K Flash / 20K RAM, and inspecting `build/bluepill.map` for symbol placement.

## Toolchain

Requires `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`, `openocd`, `make`. On WSL: `sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi gdb-multiarch openocd make`.

## Architecture

- **No standard library** — compiled with `-ffreestanding`, linked with `-nostdlib` (no libc, no libgcc). Only freestanding headers like `<stdint.h>` are usable; operations that need libgcc helpers (e.g. 64-bit division) will fail to link.
- **Boot sequence**: `startup/startup.s` defines the vector table in `.isr_vector` (initial SP = `_estack`, then `Reset_Handler`). `Reset_Handler` copies `.data` from Flash (`_sidata`) to RAM (`_sdata`..`_edata`), zeros `.bss` (`_sbss`..`_ebss`), calls `main()`, and spins in `hang` if it returns.
- **Vector table only covers the 16 Cortex-M3 core exceptions**, all pointing to `Default_Handler` (infinite loop). Using any peripheral interrupt (EXTI, TIM, USART, …) requires extending the table in `startup.s` with the STM32F103 device IRQ entries and adding the handler symbol.
- **Linker script** (`linker.ld`): Flash at 0x08000000 (64K), RAM at 0x20000000 (20K). `_estack` = top of RAM. `.isr_vector` is `KEEP`'d at the start of `.text` (required since `--gc-sections` is on). No heap section.
- **Clock**: no clock configuration is done — the chip runs on the default 8 MHz HSI. The `delay()` busy-loop timing depends on this and on `-O0`.
- **Peripherals** are accessed via memory-mapped register macros defined in `src/main.c` (base addresses and offsets from RM0008).

## Conventions

- Register macros use the pattern `(*(volatile uint32_t *)(BASE + offset))`; bit masks are separate `#define`s. Modify registers with read-modify-write (`&= ~mask` then `|=`) so other bits are preserved.
- Source comments are written in Chinese and are dense/explanatory (this is a learning project) — match that style.
- Compiler flags: `-mcpu=cortex-m3 -mthumb -g3 -O0` plus `-ffunction-sections -fdata-sections` with `--gc-sections`.
- **Adding a source file**: the Makefile lists objects and rules explicitly. Add the file to `C_SOURCES`/`ASM_SOURCES`, add its `.o` to `OBJECTS`, and add a matching `$(BUILD)/x.o:` rule.
- Build artifacts go in `build/`.
