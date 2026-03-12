# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EtherCAT slave firmware for the **MIMXRT1189 EVK** (NXP i.MX RT1100, dual-core ARM Cortex-M33 + M7). Uses NXP's **GOAL framework** (Generic Object Abstraction Layer) to implement an EtherCAT slave with 2 TxPDO inputs (slave→master) and 2 RxPDO outputs (master→slave).

The `GOAL/` directory contains **ICC-generated files** (NXP Industrial Communication Creator output) — treat these as the source of truth for the EtherCAT object dictionary. The reference application lives in `OLD/rd-riop-ecat-1.5.0/riop_ECAT/`.

## Build System

**Toolchain:** ARM GCC via CMake + Ninja. Requires `SdkRootDirPath` environment variable pointing to `mcuxsdk/mcuxsdk/`.

**Primary IDE:** VS Code with MCUXpresso for VS Code extension (v25.03+).

### Build commands (M7FOLLOWER — main application)

```bash
cd OLD/rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER

export SdkRootDirPath=/c/Projects/ICCTest/mcuxsdk/mcuxsdk
export BOARD=evkmimxrt1180
export CORE_ID=m7

cmake --preset=debug
cmake --build --preset=debug
```

Output binary: `OLD/rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/debug/riop_ECAT_M7FOLLOWER.elf`

### Build order matters

1. **EEPROM_WRITER** — run once on new hardware to program ESC EEPROM
2. **M33LEADER** — must be flashed first; initializes hardware and starts the M7 core
3. **M7FOLLOWER** — main EtherCAT application

## Architecture

### Dual-core split

| Core | Project | Role |
|------|---------|------|
| Cortex-M33 | `M33LEADER/` | Boot, hardware init, clock config, starts M7 |
| Cortex-M7 | `M7FOLLOWER/` | FreeRTOS, GOAL stack, EtherCAT PDO loop |

The PDO callbacks run on the M33 (network core). To touch hardware from a PDO callback, pack the value into a command struct and send it to the M7 (I/O core) via RPMsg. The M7 runs a FreeRTOS task that dequeues the command and calls the GPIO/peripheral driver.

### M7FOLLOWER application structure (`source/`)

- `icc_task/` — EtherCAT / GOAL stack task
- `afe_task/` — NAFE13388 analog front-end (ADC, current/voltage/temperature)
- `gpio_task/` — GPIO and LED control
- `SIGGEN_task/` — Signal generator
- `appl/goal_ecat/riop/` — ICC-generated GOAL files (object dictionary, callbacks)

### EtherCAT object dictionary (ICC-generated)

| Object | Type | Direction | Purpose |
|--------|------|-----------|---------|
| 0x6000 | ARRAY (2 sub-indices) | TxPDO (slave→master) | Input values (ADC) |
| 0x7000 | ARRAY (2 sub-indices) | RxPDO (master→slave) | Control signals (LED/GPIO) |
| 0x1A00 | PDO mapping | — | TxPDO map → 0x6000:01, 0x6000:02 |
| 0x1600 | PDO mapping | — | RxPDO map → 0x7000:01, 0x7000:02 |

Objects use **Managed Variables** — the GOAL stack owns storage; use the generated API to read/write them. ARRAY was chosen over RECORD because ICC locks sub-index count on RECORD types.

### Key ICC-generated files

- `GOAL/goal_appl_ecat_objects.c/.h` — Object dictionary definitions and `appl_ecatCreateObjects()`
- `GOAL/goal_appl_ecat.c/.h` — EtherCAT callbacks; **PDO exchange logic goes here**
- `GOAL/goal_appl.c` — Top-level GOAL app init (preserve any extra init from example project when merging)
- `GOAL/ecat_conf.h` — EtherCAT stack compile-time configuration
- `GOAL/ICCTest_ESI.xml` — EtherCAT Slave Information file for TwinCAT import

## How GOAL EtherCAT wiring works

GOAL abstracts the EtherCAT stack — you never write protocol code. The pattern is:

**Step 1 — Declare C variables** (one per PDO entry):
```c
uint8_t my_output_1;   // master writes this (RxPDO, 0x7000 range)
uint8_t my_input_1;    // master reads this  (TxPDO, 0x6000 range)
```

**Step 2 — Register them in `appl_ecatCreateObjects()`:**
```c
goal_ecatdynOdSubIndexAdd(
    pHdlEcat,
    0x7000,                          // index (RxPDO: 0x7000+, TxPDO: 0x6000+)
    0x01,                            // sub-index (0x00 = count, data starts at 0x01)
    GOAL_ECAT_DATATYPE_UNSIGNED8,
    EC_OBJATTR_RD | EC_OBJATTR_WR | EC_OBJATTR_RXPDOMAPPING | ...,
    &defaultVal, &minVal, &maxVal,
    1,                               // size in bytes
    (uint8_t *) &my_output_1);       // pointer binding — only link between index and memory
```

**Step 3 — Act in PDO callbacks in `appl_ecatCallback()`:**
```c
// Fires after stack writes new master→slave data into RxPDO variables
static void appl_ecatPdoReceived(GOAL_ECAT_T *pHdlEcat) {
    if (my_output_1) { /* drive hardware / send RPMsg to M7 */ }
}

// Fires before stack sends slave→master data; populate TxPDO variables here
static void appl_ecatPdoTxPrepare(GOAL_ECAT_T *pHdlEcat) {
    my_input_1 = read_my_sensor();
}
```

**Naming trap:** EtherCAT names things from the slave's perspective.
- RxPDO = *received by slave* = master is writing = your **outputs**
- TxPDO = *transmitted by slave* = master is reading = your **inputs**
- TwinCAT shows these flipped in its process image.

**Index/sub-index conventions:**

| Range | Conventional use |
|-------|-----------------|
| 0x6000–0x6FFF | TxPDO-mappable objects (slave inputs → master reads) |
| 0x7000–0x7FFF | RxPDO-mappable objects (slave outputs ← master writes) |
| 0x1600–0x17FF | RxPDO mapping objects |
| 0x1A00–0x1BFF | TxPDO mapping objects |

## Integration Plan

The ICC-generated files in `GOAL/` are **unmodified originals** that need to be merged into the example project:
1. Copy the example RIOP project into this directory
2. Remove conflicting build/source files from the example
3. Apply ICC-generated files on top
4. Preserve any extra initialization in the example's `goal_appl.c` (networking, device detection) — do not blindly overwrite

## Pending code changes (not yet applied)

In `GOAL/goal_appl_ecat.c`, in the cyclic PDO callbacks:
- `appl_ecatPdoTxPrepare`: write a dummy counter or ADC value to `0x6000:01` / `0x6000:02`
- `appl_ecatPdoReceived`: read `0x7000:01` / `0x7000:02` and toggle an onboard LED/GPIO (via RPMsg to M7)

## Hardware / Deployment notes

- **Vendor ID** must match in both `ICCTest_ESI.xml` and the ESC EEPROM — changing it in ICC only updates the ESI; reprogram EEPROM with EEPROM_WRITER afterward
- **TwinCAT setup:** Import `ICCTest_ESI.xml` into TwinCAT *before* scanning for the slave; verify SM2 (RxPDO) and SM3 (TxPDO) assignments match the PDO mapping objects
- TwinCAT has known issues on Windows 11 — contact Beckhoff for support
