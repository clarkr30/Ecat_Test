# Changes Required to Get 2-DI/2-DO EtherCAT Working

This document summarizes every change made from the ICC-generated glue files and original NXP example to produce a working 2 digital input / 2 digital output EtherCAT slave.

---

## 1. `goal_appl_ecat_objects.c` — Fix PDO Mapping Entries

**Problem:** ICC generates `0x1600` (RxPDO) and `0x1A00` (TxPDO) mapping entries with value `0x00000000`. The GOAL stack uses these entries to bind the PDO buffer to object variables. With zero values, `di1_in`/`di2_in` are never written from incoming PDO data, and `di1_out`/`di2_out` are never read into the outgoing PDO buffer. The slave reaches OP state but I/O is completely silent.

**Fix:** Set the mapping entry values to the correct encoding `(index << 16) | (subindex << 8) | bitlen`, and set Number of Entries to 3 (not 1). Also add the missing subindex 0x02 and 0x03 entries for both mapping objects.

| Object | Subindex | Before | After |
|--------|----------|--------|-------|
| 0x1600:00 | Number of Entries | `0x01` | `0x03` |
| 0x1600:01 | Output 1 mapping | `0x00000000` | `0x70000108` (0x7000:01, 8-bit) |
| 0x1600:02 | Output 2 mapping | *(missing)* | `0x70000208` (0x7000:02, 8-bit) |
| 0x1600:03 | LED mapping | *(missing)* | `0x70000308` (0x7000:03, 8-bit) |
| 0x1A00:00 | Number of Entries | `0x01` | `0x03` |
| 0x1A00:01 | Input 1 mapping | `0x00000000` | `0x60100108` (0x6010:01, 8-bit) |
| 0x1A00:02 | Input 2 mapping | *(missing)* | `0x60100208` (0x6010:02, 8-bit) |
| 0x1A00:03 | LED mapping | *(missing)* | `0x60100308` (0x6010:03, 8-bit) |

---

## 2. `goal_appl_ecat.c` — Fill In PDO Callbacks

**Problem:** ICC generates empty stub callbacks. The callbacks need to be filled in to transfer data between the EtherCAT object variables and the RIOP hardware layer.

**Fix:** Implement `appl_ecatPdoReceived` and `appl_ecatPdoTxPrepare`. Add `#include "gl.h"` for access to the shared variables.

### `appl_ecatPdoReceived` (TwinCAT → GPIO outputs)

```c
#include "gl.h"   // add at top of file

static void appl_ecatPdoReceived(GOAL_ECAT_T *pHdlEcat)
{
    UNUSEDARG(pHdlEcat);

    fsm_mode = 5;   /* MCU output mode: 20ms dispatch */

    uint8_t *pii = &riop_command.mcu_digital_output_command.mcu_digital_output;
#define ATOMIC_OP(var, XXX) if(var) *pii|=(uint8_t)XXX; else *pii&=~(uint8_t)XXX
    ATOMIC_OP(di1_in, 0x01);   /* -> DIGITAL_OUTPUT_01 (RGPIO2 pin 21, K4) */
    ATOMIC_OP(di2_in, 0x02);   /* -> DIGITAL_OUTPUT_02 (RGPIO2 pin 22, L3) */
#undef ATOMIC_OP

    LED_status = led_in;
    led_green = LED_status;
}
```

**Key points:**
- `fsm_mode = 5` enables the 20ms dispatch loop in `digital_io.c`. Without this the outputs only update every ~1 second (mode 0) or not at all.
- Do NOT set `riop_command.mcu_digital_output_command.is_dirty = 1` here — the `digital_io.c` case-5 change detector handles that. Setting it here causes a race condition.
- Bitmask `0x01` = DIGITAL_OUTPUT_01, `0x02` = DIGITAL_OUTPUT_02 (bit position in RGPIO2 starting at pin 21).

### `appl_ecatPdoTxPrepare` (GPIO inputs → TwinCAT)

```c
static void appl_ecatPdoTxPrepare(GOAL_ECAT_T *pHdlEcat)
{
    UNUSEDARG(pHdlEcat);

    uint8_t *pii = &g_boardStatus.mcu_digital_input_pins_status;
#define ATOMIC_OP(var, XXX) if(*pii&(uint8_t)XXX) var=1; else var=0
    ATOMIC_OP(di1_out, 0x01);  /* DIGITAL_INPUT_01 (RGPIO6 pin 12, A11) */
    ATOMIC_OP(di2_out, 0x02);  /* DIGITAL_INPUT_02 (RGPIO6 pin 8,  B14) */
#undef ATOMIC_OP

    led_out = LED_status;
}
```

**Key point:** `g_boardStatus` is populated by `ICC_DataHandleTask` → `RIOP_GetBoardStatusForICC()` on every `appl_loop` cycle, which reads from `gpio_task.c`'s `RIOP_SetMcuDigitalInputPinsStatus()`.

---

## 3. `goal_appl.c` — Initialize RIOP and Fix PDO Buffer Size

**Problem:** ICC generates `goal_appl.c` without the RIOP hardware initialization call and with PDO buffer size 0.

**Fix:**

```c
#define APPL_ECAT_PDO_BUF_SIZE 8   /* was 0 */

extern int riop_appl_init(void);   /* add this declaration */

GOAL_STATUS_T appl_init(void)
{
    riop_appl_init();              /* add this call first */
    // ... rest of init
}

GOAL_STATUS_T appl_setup(void)
{
    // add bootstrap support:
    res = goal_ecatCfgBootstrapOn(GOAL_TRUE);
    // ... rest of setup
}
```

---

## 4. `ecat_conf.h` — Use CC Library Version

**Problem:** ICC generates `ecat_conf.h` with wrong SM addresses and missing defines required by the RPC build.

**Fix:** Always use the CC library reference version:
```
rd-riop-ecat-1.5.0/goal/projects/goal_ecat_rpc_lib/00_cc/edt/nxp/evkmimxrt1180/ecat_conf.h
```

Correct SM addresses (fixed by the pre-built GOAL RPC library):
| SM | Address | Size |
|----|---------|------|
| SM0 MBoxOut | `0x1000` | `0x0080` |
| SM1 MBoxIn  | `0x1080` | `0x0080` |
| SM2 RxPDO   | `0x1100` | 3 bytes  |
| SM3 TxPDO   | `0x1400` | 3 bytes  |

---

## 5. `ICCTest_ESI.xml` — Fix SM Addresses and PDO Entries

**Problem:** ICC generates ESI with wrong SM addresses and empty PDO entry placeholders. TwinCAT uses the ESI to configure FMMUs and determine the process image layout.

**Fix the SM addresses** (in the `<Sm>` section):
```xml
<Sm MinSize="128" MaxSize="128" DefaultSize="128" StartAddress="#x1000" .../>  <!-- MBoxOut -->
<Sm MinSize="128" MaxSize="128" DefaultSize="128" StartAddress="#x1080" .../>  <!-- MBoxIn  -->
<Sm DefaultSize="3" StartAddress="#x1100" .../>  <!-- RxPDO -->
<Sm DefaultSize="3" StartAddress="#x1400" .../>  <!-- TxPDO -->
```

**Fix the PDO entries** (replace empty `<Entry>` placeholders):
```xml
<RxPdo Fixed="1" Sm="2">
    <Index>#x1600</Index>
    <Entry><Index>#x7000</Index><SubIndex>#x01</SubIndex><BitLen>8</BitLen><DataType>USINT</DataType><Name>Output 1</Name></Entry>
    <Entry><Index>#x7000</Index><SubIndex>#x02</SubIndex><BitLen>8</BitLen><DataType>USINT</DataType><Name>Output 2</Name></Entry>
    <Entry><Index>#x7000</Index><SubIndex>#x03</SubIndex><BitLen>8</BitLen><DataType>USINT</DataType><Name>led</Name></Entry>
</RxPdo>
<TxPdo Fixed="1" Sm="3">
    <Index>#x1A00</Index>
    <Entry><Index>#x6010</Index><SubIndex>#x01</SubIndex><BitLen>8</BitLen><DataType>USINT</DataType><Name>Input 1</Name></Entry>
    <Entry><Index>#x6010</Index><SubIndex>#x02</SubIndex><BitLen>8</BitLen><DataType>USINT</DataType><Name>Input 2</Name></Entry>
    <Entry><Index>#x6010</Index><SubIndex>#x03</SubIndex><BitLen>8</BitLen><DataType>USINT</DataType><Name>led</Name></Entry>
</TxPdo>
```

After updating the ESI, copy it to `C:\Program Files (x86)\Beckhoff\TwinCAT\3.1\Config\Io\EtherCAT\` and delete+rescan the device in TwinCAT.

---

## 6. EEPROM SII — Fix SM Addresses

**Problem:** EEPROM has SM addresses in two places. The SM category section (offset ~0xD0) had old wrong addresses (SM1@0x1400, SM2@0x1800, SM3@0x1C00). Also the original array was 224 bytes — the 8th 32-byte page containing the SM category was never written.

**Fix:** Rewrite `ecat_io_eeprom.h` as an explicit flat 256-byte array with correct values:

- Bytes 0x028–0x02B: SM0 addr `0x1000`, size `0x0080`
- Bytes 0x02C–0x02F: SM1 addr `0x1080`, size `0x0080`
- SM category at offset 0xD0+:
  - SM0: addr=`0x1000`, size=`0x0080`, ctrl=`0x26`, activate=`0x01`
  - SM1: addr=`0x1080`, size=`0x0080`, ctrl=`0x22`, activate=`0x01`
  - SM2: addr=`0x1100`, size=`0x0003`, ctrl=`0x64`, activate=`0x01`
  - SM3: addr=`0x1400`, size=`0x0003`, ctrl=`0x20`, activate=`0x01`

Verify correct write by checking COM7 output shows `Write started for (8*32) = 256 bytes` and `EEPROM write success!!!`.

---

## 7. TwinCAT PLC Task Cycle Time

**Problem:** Default PLC task cycle of 100ms causes SM watchdog timeout — the EtherCAT slave drops from OP to ERR SAFEOP because PDO frames stop arriving within the watchdog period.

**Fix:** Set PlcTask cycle time to **10ms** or less (4ms recommended).

In TwinCAT: System → Tasks → PlcTask → Cycle ticks → set to give ≤10ms cycle.

---

## Build Order Reminder

**Always build M7FOLLOWER before M33LEADER.** M33LEADER embeds the M7 binary at link time. Building only M33 silently uses the stale M7 binary.

1. Build M7FOLLOWER
2. Build M33LEADER
3. Flash M33LEADER only (contains both cores)
4. Flash EEPROM_WRITER separately when EEPROM needs updating
