# PolarFire SoC Bare Metal Development
Bare metal firmware and RTL projects for the **Microchip PolarFire SoC (MPFS095T-FCSG325)**, built as part of an FPGA developer onboarding program.
> ⚠️ **Emulation Notice:** All firmware in this repository has been tested on **Renode** (hardware emulator bundled with SoftConsole). Physical hardware testing on the MPFS095T board is a future milestone.
---
## Hardware
| Item | Detail |
|---|---|
| Device | Microchip MPFS095T-FCSG325 |
| Family | PolarFire SoC |
| Processor | 5× RISC-V (1× E51 monitor + 4× U54 application cores) @ 667MHz |
| ISA | RV64GC |
| Memory | 2MB L2 Cache + external LPDDR4 |
## Tools
| Tool | Version | Purpose |
|---|---|---|
| Libero SoC | 2025.2 | RTL synthesis, P&R, simulation |
| SoftConsole | v2022.2-RISC-V-747 | Eclipse-based C/C++ IDE |
| RISC-V GCC | 8.3.0 | `riscv64-unknown-elf-gcc` |
| Renode | 1.13.1 | Full PolarFire SoC emulation (bundled with SoftConsole) |
---
## Projects
### [01_led_blink](./01_led_blink/)
RTL LED blink in Verilog — the classic FPGA Hello World.
- 50MHz fabric clock, 1Hz blink
- Synthesized and simulated in Libero SoC + ModelSim

### [02_uart_hello_world](./02_uart_hello_world/)
Bare metal C firmware — UART Hello World from scratch.
- No OS, no HAL library, no BSP
- Hand-written startup assembly (`startup.S`)
- Custom linker script (`pfsoc.ld`)
- 16550-compatible UART driver
- **Verified working on Renode emulator**

### [03_gpio](./03_gpio/)
Bare metal GPIO driver — LED blink from software.
- Read-modify-write CFG register for pin direction
- GPIO OUT register for pin control
- 5x LED blink verified on Renode

### [04_timer](./04_timer/)
Bare metal RISC-V CLINT timer driver with interrupt support.
- Direct mtime/mtimecmp register access (no HAL)
- Safe 64-bit read with HI/LO rollover detection
- Spurious interrupt prevention via 0xFFFFFFFF HI guard write
- Callback function pointer pattern for ISR dispatch
- mstatus.MIE + mie.MTIE CSR configuration via inline assembly
- **Timer init and mtimecmp register writes verified on Renode**

### 05_linux_boot *(future)*
---
## Quick Start — Run on Renode (No Hardware Needed)
### Prerequisites
- [SoftConsole v2022.2](https://www.microchip.com/en-us/products/fpgas-and-plds/fpga-and-soc-design-tools/soc-fpga/softconsole) (includes Renode + RISC-V GCC)
- Windows 10/11
### 1. Clone
```
git clone https://github.com/kazmih/pfsoc-baremetal.git
```
### 2. Build
Open SoftConsole, import `02_uart_hello_world` as a C project and build. Or from command line:
```
cd 02_uart_hello_world
riscv64-unknown-elf-gcc -march=rv64gc -mabi=lp64d -mcmodel=medany -O0 -g3 \
  -nostartfiles -nostdlib -static -T pfsoc.ld -I hal -I src \
  startup/startup.S src/uart.c src/main.c -o pfsoc_baremetal.elf
```
### 3. Emulate
```
"F:\SoftConsole\renode\bin\Renode.exe" --plain -e "include @02_uart_hello_world/run.resc"
```
Expected UART output:
```
================================
  PolarFire SoC Bare Metal
  Hello World from RISC-V!
================================
UART driver working correctly.
Baud rate: 115200, Format: 8N1
APB clock: 150MHz, Divisor: 81
```
### 4. Real Hardware *(future)*
Flash via SoftConsole + OpenOCD to the MPFS095T board.
---
## Roadmap
```
✅ 01 — LED blink RTL (Libero SoC)
✅ 02 — UART bare metal (Renode verified)
✅ 03 — GPIO driver (Renode verified)
✅ 04 — RISC-V CLINT timer + interrupts (Renode verified)
🔜 05 — Linux boot: HSS → U-Boot → OpenSBI → Kernel
🔜 06 — Physical hardware testing on MPFS095T
```