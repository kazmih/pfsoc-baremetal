# Renode Emulation Guide

This guide explains how to run PolarFire SoC bare metal firmware on Renode
without any physical hardware.

---

## What is Renode?

Renode is a hardware emulator that simulates the entire SoC — CPU cores,
memory, and peripherals. When your code writes to address `0x20000000`
(UART TX register), Renode intercepts that write and displays the character
in a terminal window. From your firmware's perspective, it is talking to
real hardware.

Renode is bundled with SoftConsole at:
```
F:\SoftConsole\renode\bin\Renode.exe
```

---

## Files Required

| File | Purpose |
|---|---|
| `pfsoc_simple.repl` | Platform description — tells Renode what hardware exists |
| `run.resc` | Renode script — loads ELF, opens UART window, starts emulation |
| `pfsoc_baremetal.elf` | Your compiled firmware |

---

## pfsoc_simple.repl — Platform Description

```
cpu: CPU.RiscV64 @ sysbus
    cpuType: "rv64gc"
    timeProvider: empty

lim: Memory.MappedMemory @ sysbus 0x08000000
    size: 0x40000

dtim: Memory.MappedMemory @ sysbus 0x01000000
    size: 0x2000

uart: UART.NS16550 @ sysbus 0x20000000
    -> cpu@0
```

This creates a minimal PolarFire SoC environment:
- One RV64GC CPU core starting at reset
- 256KB of LIM (Loosely Integrated Memory) at 0x08000000 — where our code lives
- 8KB of DTIM at 0x01000000 — where our stack and data live
- A 16550-compatible UART at 0x20000000 — our serial output

---

## run.resc — Renode Script

```
mach create "pfsoc"
machine LoadPlatformDescription @pfsoc_simple.repl
sysbus LoadELF @pfsoc_baremetal.elf
showAnalyzer sysbus.uart
start
```

Line by line:
1. Create a machine called "pfsoc"
2. Load the platform description (hardware definition)
3. Load our ELF binary into the simulated memory
4. Open a UART terminal window
5. Start the emulation

---

## How to Run

### Step 1 — Build the firmware
In SoftConsole: right-click project → Build Project

### Step 2 — Run Renode
Open Command Prompt and run:
```
"F:\SoftConsole\renode\bin\Renode.exe" --plain -e "include @F:\path\to\run.resc"
```

### Step 3 — View UART output
A window titled `pfsoc:sysbus.uart` opens automatically.
All `uart_puts()` output appears here.

---

## What the Logs Mean

```
sysbus: Loading segment of 1012 bytes at 0x8000000   ← .text section loaded
sysbus: Loading segment of 4096 bytes at 0x80003F4   ← .bss section (stack)
cpu: Setting PC value to 0x8000000                   ← CPU reset vector set
pfsoc: Machine started.                              ← emulation running
```

---

## Debugging in Renode

The Renode console accepts commands while the machine runs:

```
# Pause execution
sysbus.cpu Pause

# Read a register
sysbus.cpu PC

# Read memory at an address
sysbus ReadDoubleWord 0x08000000

# Step one instruction
sysbus.cpu Step 1

# Resume
sysbus.cpu Resume

# List all peripherals
peripherals
```

---

## Running on Real Hardware (Future)

When physical MPFS095T hardware is available:
1. Connect board via USB (creates a COM port)
2. In SoftConsole: Run → Debug Configurations → OpenOCD
3. Use the `PolarFire SoC program idle boot mode 0` launch configuration
4. SoftConsole flashes the ELF via OpenOCD + JTAG

The same ELF that runs on Renode will run on real hardware —
no code changes needed.
