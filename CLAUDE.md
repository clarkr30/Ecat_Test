# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EtherCAT slave application for NXP MIMXRT1189 EVK (dual-core: Cortex-M33 + Cortex-M7). The goal is a slave device with 2 digital outputs (TwinCAT→GPIO pins) and 2 digital inputs (GPIO pins→TwinCAT) that reaches OP state.

The application uses GOAL (Generic Object Abstraction Layer) EtherCAT stack from port GmbH, accessed as a pre-built RPC library (`goal_ecat_rpc_lib`). The M33 runs the EtherCAT/network stack; the M7 handles I/O and communicates with M33 via shared memory.

## Build System

CMake with Ninja, ARM GCC toolchain, MCUXpresso SDK v25.09.00.

**Build order is critical — M7FOLLOWER must be built before M33LEADER.** M33LEADER embeds the M7 binary (`core1_image.bin`) at link time. Building only M33 will silently use the old M7 binary.

```bash
# From MCUXpresso VS Code extension, or directly:
# 1. Build M7FOLLOWER
cd rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER
cmake --preset debug
cmake --build build/debug

# 2. Build M33LEADER (embeds M7 binary)
cd ../M33LEADER
cmake --preset debug
cmake --build build/debug

# 3. Flash M33LEADER only (it contains both cores)
```

Build presets (`debug`, `release`, `flexspi_nor_debug`, `flexspi_nor_release`) are defined in `CMakePresets.json` in each project directory. SDK path is configured in `mcux_include.json` (`c:/w/SDK/MCUXpresso/v25.09.00`).

There is also an EEPROM_WRITER project (`riop_ECAT/EEPROM_WRITER/`) that must be built and flashed separately when restoring or updating EEPROM.

## Architecture

### Multi-core split
- **M33LEADER** (`riop_ECAT/M33LEADER/`): EtherCAT master stack, lwIP networking, EEPROM identity, flash management
- **M7FOLLOWER** (`riop_ECAT/M7FOLLOWER/`): AFE task (NAFE13388 ADC), GPIO task, signal generator, ICC task, digital I/O

### Application files to modify
All EtherCAT application logic lives in:
```
rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/
```
Key files:
- `goal_appl_ecat_objects.c/.h` — CoE object dictionary (0x1000, 0x1018, PDO objects 0x6000/0x7000, mapping 0x1600/0x1A00)
- `goal_appl_ecat.c` — SDO/PDO callbacks: `appl_ecatPdoReceived`, `appl_ecatPdoTxPrepare`, `appl_ecatSdoUpload`, `appl_ecatSdoDownload`
- `goal_appl.c` — Top-level GOAL stack initialization

The `glue/` directory at repo root contains in-progress versions of these files (the custom 2-DI/2-DO target application).

### ICC Tool Directories

Two ICC (Industrial Communication Creator) tool project directories exist at the repo root:

- **`glue/`** — Custom ICC project (`ICCTest.iccproj`) for the 2-DI/2-DO target device. Generated files here are candidates to copy into `M7FOLLOWER/source/appl/goal_ecat/riop/`. Note: this project had warnings ("No CPU family set", "No EEPROM set") on last generation.
- **`RIOP-ECAT-APP-GLUE-V1.5.0/`** — NXP-provided ICC project (`ecat_io.iccproj`) representing the full RIOP platform (32 DI channels + 16 ADC channels). Useful as a reference for how NXP structures large object dictionaries. **Do not use its `ecat_conf.h`** — it has incorrect SM addresses (SM0@0x1000/SM1@0x1400/SM2@0x1800) for the goal_ecat_rpc_lib build.

When the ICC tool generates code for `glue/`, the output files (`ecat_conf.h`, `goal_appl_ecat_objects.c/.h`, etc.) must be reviewed before copying to the firmware — in particular, `ecat_conf.h` must always be replaced with the CC library reference version.

### EEPROM
`riop_ECAT/EEPROM_WRITER/source/ecat_io_eeprom.h` — defines the SII binary embedded in EEPROM_WRITER firmware.

## Critical EtherCAT Constraints

### SM addresses (for this pre-built CC library build)
These are fixed by the GOAL RPC library — do NOT use ICC-generated or guessed values:
- SM0 (MBoxOut): addr=`0x1000`, size=`0x0080`
- SM1 (MBoxIn):  addr=`0x1080`, size=`0x0080`
- SM2 (RxPDO):   addr=`0x1100`, size=5 bytes
- SM3 (TxPDO):   addr=`0x1400`, size=5 bytes

### ecat_conf.h
Always use the CC library reference version from:
```
rd-riop-ecat-1.5.0/goal/projects/goal_ecat_rpc_lib/00_cc/edt/nxp/evkmimxrt1180/ecat_conf.h
```
**Never replace this with an ICC-generated `ecat_conf.h`** — ICC generates wrong SM addresses and omits required defines for the RPC build.

### EEPROM SII SM section
The EEPROM has two places for mailbox SM addresses. TwinCAT uses the **SII SM category section** (around offset `0xD0+`), not just bytes 40-47. Both must be consistent and must match the SM addresses above.

### 0x1018 identity vs EEPROM
The GOAL stack checks `0x1018` (VendorId/ProductCode/RevisionNo) against the EEPROM SII during INIT→PREOP. A mismatch causes silent rejection (no AL status error code).

### FoE callback
`goal_appl_foe.c` is not compiled in M7FOLLOWER's CMakeLists.txt. If using `04_led_button_eoe` as a reference, remove or stub `appl_ecatFoeCallback`.

## Serial / Debugging

Only COM7 (CH340 USB-Serial) is available — this is M33 UART only. M7 has no visible serial output.

Normal startup output (does not confirm M7 is working):
```
Start the digital_io example. (ETHERCAT) [timestamp]
Secondary Core Started...
RIOP Application Initialization..
```

`AL Status Code 0x0000` in TwinCAT with INIT→PREOP timeout means TwinCAT timed out, not that the slave reported clean — the slave (usually M7) simply never responded.

## Reference Example

`rd-riop-ecat-1.5.0/goal/appl/goal_ecat/04_led_button_eoe/` is the working reference for LED/button PDO mapping. Use as base for new PDO objects — but exclude `goal_appl_foe.c`.

The original unmodified `rd-riop-ecat-1.5.0` example reaches OP with TwinCAT. When in doubt, revert to it and confirm OP before making changes.

## Device Identity

FACTS Engineering LLC device:
- VendorId: `0xE778` (59256)
- ProductCode: `0x0001`
- RevisionNo: `0x0011`

Original NXP values (simpler baseline): ProductCode=`0x67`, RevisionNo=`0x68`
