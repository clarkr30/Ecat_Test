# EtherCAT I/O Pin Mapping

This document traces each TwinCAT PDO variable from its CoE object through the
firmware layers down to the physical pin on the MIMXRT1180-RIOP board.

All source references are relative to
`rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/`.

---

## Data Path Explanation

### Overview

The MIMXRT1189 has two cores. M33 runs the EtherCAT stack; M7 owns the I/O.
Both run independent FreeRTOS schedulers. The EtherCAT PDO callback functions
run on M7 (compiled into `M7FOLLOWER`) and communicate with the hardware tasks
via a global command struct and FreeRTOS queues.

```
TwinCAT master
     │
     │  100BASE-TX EtherCAT frame
     │
     ▼
[ M33LEADER ]
  GOAL EtherCAT RPC library
  Unpacks PDO frame, updates shared PDO variables over ICC (inter-core)
     │
     │  Inter-Core Communication (ICC) — shared OCRAM
     │
     ▼
[ M7FOLLOWER ]
  ┌─────────────────────────────────────────────────────────────────────────────┐
  │  EtherCAT Application Glue                                                  │
  │  source/appl/goal_ecat/riop/goal_appl_ecat.c                                │
  │  source/appl/goal_ecat/riop/goal_appl_ecat_objects.c  (PDO var declarations)│
  │                                                                              │
  │  appl_ecatPdoReceived()  ← called by GOAL stack on each RxPDO              │
  │    writes directly into global riop_command                                 │
  │    (riop_command defined in source/digital_io.c:43)                         │
  │    (riop_command_t struct defined in source/api_riop.h:106)                 │
  │    sets is_dirty flags on changed fields                                     │
  │                                                                              │
  │  appl_ecatPdoTxPrepare() ← called by GOAL stack on each TxPDO              │
  │    reads from global g_boardStatus                                           │
  │    (g_boardStatus defined in source/icc_task/icc_task.c:51)                 │
  │    (riop_board_status_t struct defined in source/api_riop.h:165)            │
  └────────────────────┬────────────────────────────────────────────────────────┘
                       │
                       │  appl_loop() polls is_dirty flags and calls
                       │  ICC_DataHandleTask()
                       │  (source/digital_io.c:111, called at line 340)
                       │
                       ▼
  ┌─────────────────────────────────────────────────────────────────────────────┐
  │  ICC_DataHandleTask()                                                        │
  │  source/icc_task/icc_task.c:68                                              │
  │  (called directly from appl_loop — not a FreeRTOS task)                    │
  │                                                                              │
  │  Checks is_dirty flags, dispatches to FreeRTOS queues:                     │
  │   .mcu_digital_output_command.is_dirty → gpio_command_queue  (line 89-92)  │
  │   .afe_command.*is_dirty              → afe_command_queue    (line 94-101) │
  │   .siggen_control_command.is_dirty    → siggen_command_queue (line 84-87)  │
  │                                                                              │
  │  Collects status for TxPDO:                                                 │
  │   RIOP_GetBoardStatusForICC(&g_boardStatus)  (line 107)                    │
  │   (defined in source/api_riop.c)                                            │
  └────────┬─────────────────────────────────────────────┬──────────────────────┘
           │                                             │
           ▼                                             ▼
  ┌──────────────────────────┐             ┌──────────────────────────────────┐
  │  GPIO_ControlTask        │             │  AFE_Task                        │
  │  source/gpio_task/       │             │  source/afe_task/afe_task.c      │
  │    gpio_task.c           │             │                                  │
  │                          │             │  Dequeues afe_command_queue      │
  │  Dequeues                │             │  → NAFE_gpioConfig() +           │
  │  gpio_command_queue      │             │    NAFE_gpioWriteData()          │
  │  → RGPIO_PinWrite        │             │    (GP direction + output value) │
  │    (RGPIO2, i+21, ...)   │             │  via SPI on LPSPI1               │
  │  for outputs di17–di24   │             │  (board/board.h:BOARD_NAFE_SPI)  │
  │                          │             │                                  │
  │  Reads RGPIO6 pins       │             │  Reads NAFE13388 GPIO inputs     │
  │  (board/board.h:         │             │  + all ADC channels              │
  │   BOARD_DIG_IN1–7)       │             │  → RIOP_SetAfeDigitalInput       │
  │  → RIOP_SetMcuDigital    │             │    PinsStatus()                  │
  │    InputPinsStatus()     │             │  (source/api_riop.c)             │
  │  (source/api_riop.c)     │             │                                  │
  │                          │             │  AFE channel config defined in   │
  │  50 ms poll cycle        │             │  source/riop_feature_config.h    │
  │  (gpio_task.c:104)       │             │                                  │
  └──────────────────────────┘             └──────────────────────────────────┘
           │                                             │
           ▼                                             ▼
  RGPIO2 pins 21–28                          NAFE13388 over LPSPI1
  (board/pin_mux.h:                          GP2–GP9, HV/LV/current/temp ADC
   DIGITAL_OUTPUT_01–08)
  RGPIO6 pins 6–10, 12–13
  (board/pin_mux.h:
   DIGITAL_INPUT_01–07)
```

### RxPDO flow (TwinCAT → pins)

1. TwinCAT writes a process data frame each EtherCAT cycle.
2. M33 GOAL stack receives it and updates the PDO variables in shared memory via ICC.
3. M7 [`appl_ecatPdoReceived()`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L273)
   is called. It writes fields of the global [`riop_command`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/digital_io.c#L43)
   (typed as [`riop_command_t`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/api_riop.h#L106))
   and sets the appropriate `is_dirty` flag.
4. [`appl_loop()`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/digital_io.c#L111)
   runs on a DWT timer. When it fires, it calls
   [`ICC_DataHandleTask()`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/icc_task/icc_task.c#L68),
   which checks each `is_dirty` flag and sends the relevant sub-struct to the
   appropriate FreeRTOS queue.
5. [`GPIO_ControlTask`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c#L40)
   or [`AFE_Task`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/afe_task/afe_task.c#L182)
   dequeues the command and drives the physical pin.

**Latency note:** The hardware does not respond immediately when TwinCAT writes
a PDO. There is up to one `tim_mode` interval (default 200 ms in mode 0, 20 ms
in modes 4/5) before `appl_loop()` fires and dispatches the command. For MCU
digital outputs (mode 5), `is_dirty` is only set when the value actually
**changes** from the previously sent value (change-detection logic in
[digital_io.c:321–333](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/digital_io.c#L321-L333)).
`GPIO_ControlTask` then processes its queue within its 50 ms poll cycle.

### TxPDO flow (pins → TwinCAT)

1. [`GPIO_ControlTask`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c#L40)
   (50 ms) reads MCU input pins and calls
   [`RIOP_SetMcuDigitalInputPinsStatus()`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/api_riop.h#L181).
   [`AFE_Task`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/afe_task/afe_task.c#L182)
   reads NAFE13388 and calls
   [`RIOP_SetAfeDigitalInputPinsStatus()`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/api_riop.h#L183).
2. These functions write into `gRiop_status_shm` (a semaphore-protected shared
   struct in `api_riop.c`, typed as [`riop_board_status_t`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/api_riop.h#L165)).
3. [`ICC_DataHandleTask()`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/icc_task/icc_task.c#L107)
   calls `RIOP_GetBoardStatusForICC(&g_boardStatus)`, which copies
   `gRiop_status_shm` into the global
   [`g_boardStatus`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/icc_task/icc_task.c#L51).
4. [`appl_ecatPdoTxPrepare()`](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L366)
   reads `g_boardStatus` and packs the values into the PDO variables
   (declared in [goal_appl_ecat_objects.c](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat_objects.c#L31)),
   which are then sent to TwinCAT in the next TxPDO frame.

---

## How Bitmasks Map to Pins

The ATOMIC_OP bitmask is **not** the pin number directly — it is a bit position
within a packed status/command byte or word. Two separate registers are used:

### MCU digital inputs — `mcu_digital_input_pins_status` (uint8)

Packed in [gpio_task.c:65-68](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c#L65-L68):

```c
mcu_digital_input_status |= (mcu_digital_input_pin[i] << i);
```

Each physical pin is read individually via `RGPIO_PinRead` then shifted into
bit position `i`. So bit position = array index = `BOARD_DIG_IN(i+1)`:

| Bitmask | Bit | `BOARD_DIG_IN` | RGPIO6 pin | Pad | IC coord |
|---------|-----|----------------|-----------|-----|----------|
| `0x01`  | 0   | DIG_IN1        | 12        | GPIO_B1_12 | A11 |
| `0x02`  | 1   | DIG_IN2        | 8         | GPIO_B1_08 | B14 |
| `0x04`  | 2   | DIG_IN3        | 7         | GPIO_B1_07 | D13 |
| `0x08`  | 3   | DIG_IN4        | 13        | GPIO_B1_13 | B11 |
| `0x10`  | 4   | DIG_IN5        | 6         | GPIO_B1_06 | B10 |
| `0x20`  | 5   | DIG_IN6        | 9         | GPIO_B1_09 | A15 |
| `0x40`  | 6   | DIG_IN7        | 10        | GPIO_B1_10 | A13 |

### AFE GPIO inputs/outputs — `afe_gpio_input_pins_status` / `gpio_output_value` (uint16)

`NAFE_gpioReadData()` and `NAFE_gpioWriteData()` return/accept a 16-bit value
where **bit position = NAFE GP pin number**:

| Bitmask  | Bit | NAFE GP pin | Default direction |
|----------|-----|-------------|-------------------|
| `0x0004` | 2   | GP2         | input             |
| `0x0008` | 3   | GP3         | input             |
| `0x0010` | 4   | GP4         | input             |
| `0x0020` | 5   | GP5         | input             |
| `0x0040` | 6   | GP6         | output            |
| `0x0080` | 7   | GP7         | output            |
| `0x0100` | 8   | GP8         | output            |
| `0x0200` | 9   | GP9         | output            |

Default directions set at init: `gpio_pin_direction = 0x3C0` (bits 6–9 = GP6–GP9 as outputs).

### MCU digital outputs — `gpio_command_queue` byte (uint8)

Written by `GPIO_ControlTask`
([gpio_task.c:90-99](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c#L90-L99))
as `RGPIO_PinWrite(RGPIO2, i+21, ...)` — bit position `i` → RGPIO2 pin `i+21`:

| Bitmask | Bit | RGPIO2 pin | Pad |
|---------|-----|-----------|-----|
| `0x01`  | 0   | 21        | GPIO_EMC_B1_21 (DIGITAL_OUTPUT_01) |
| `0x02`  | 1   | 22        | GPIO_EMC_B1_22 (DIGITAL_OUTPUT_02) |
| `0x04`  | 2   | 23        | GPIO_EMC_B1_23 (DIGITAL_OUTPUT_03) |
| `0x08`  | 3   | 24        | GPIO_EMC_B1_24 (DIGITAL_OUTPUT_04) |
| `0x10`  | 4   | 25        | GPIO_EMC_B1_25 (DIGITAL_OUTPUT_05) |
| `0x20`  | 5   | 26        | GPIO_EMC_B1_26 (DIGITAL_OUTPUT_06) |
| `0x40`  | 6   | 27        | GPIO_EMC_B1_27 (DIGITAL_OUTPUT_07) |
| `0x80`  | 7   | 28        | GPIO_EMC_B1_28 (DIGITAL_OUTPUT_08) |

---

## Remapping PDO Variables to Different Pins

To reassign which PDO variable reads from which physical pin, move the
`ATOMIC_OP` call into the block that points at the correct hardware register
**and use the bitmask for the target pin**.

### Example: swap di1_out (AFE GP2) and di9_out (MCU DIGITAL_INPUT_01)

Original in [goal_appl_ecat.c:378-401](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L378-L401):

```c
// Block 1 — reads afe_gpio_input_pins_status (pi)
ATOMIC_OP(di1_out, 0x0004);   // AFE GP2
...
// Block 2 — reads mcu_digital_input_pins_status (pii)
ATOMIC_OP(di9_out,  0x01);    // BOARD_DIG_IN1
```

After swap — `di1_out` now reflects MCU DIGITAL_INPUT_01, `di9_out` reflects
AFE GP2:

```c
// Block 1 — reads afe_gpio_input_pins_status (pi)
ATOMIC_OP(di9_out, 0x0004);   // AFE GP2  ← was di1_out
ATOMIC_OP(di2_out, 0x0008);
...
// Block 2 — reads mcu_digital_input_pins_status (pii)
ATOMIC_OP(di1_out, 0x01);     // BOARD_DIG_IN1  ← was di9_out
ATOMIC_OP(di10_out, 0x02);
...
```

The bitmask stays tied to the hardware pin — only the PDO variable name moves.
A variable **cannot** straddle both blocks; it must live entirely in one
register's block.

---

## Outputs: TwinCAT → Hardware  (RxPDO, object 0x7000 / 0x7010)

Handled in `appl_ecatPdoReceived()`:
[goal_appl_ecat.c:273](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L273)

### 0x7000 — Digital Outputs (UINT8 each)

#### di1–di8  →  NAFE13388 GPIO output pins GP2–GP9

[goal_appl_ecat.c:284-295](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L284-L295)

```c
uint16_t *pi = &riop_command.afe_command.afe_gpio_command.gpio_output_value;
#define ATOMIC_OP(var, XXX) if(var) *pi|=(uint16_t)XXX; else *pi&=~(uint16_t)XXX
ATOMIC_OP(di1_in, 0x0004);   // bit 2 → NAFE GP2
ATOMIC_OP(di2_in, 0x0008);   // bit 3 → NAFE GP3
ATOMIC_OP(di3_in, 0x0010);   // bit 4 → NAFE GP4
ATOMIC_OP(di4_in, 0x0020);   // bit 5 → NAFE GP5
ATOMIC_OP(di5_in, 0x0040);   // bit 6 → NAFE GP6
ATOMIC_OP(di6_in, 0x0080);   // bit 7 → NAFE GP7
ATOMIC_OP(di7_in, 0x0100);   // bit 8 → NAFE GP8
ATOMIC_OP(di8_in, 0x0200);   // bit 9 → NAFE GP9
```

Dispatched via `afe_command_queue` → `AFE_Task` → `NAFE_gpioWriteData()` over
SPI (LPSPI1) to the NAFE13388 GPIO output register.

See [afe_task.c:186-202](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/afe_task/afe_task.c#L186-L202):

```c
// When afe_gpio_command.is_dirty:
afe_ctx->NafeHdl->gpioConfig->writeEnable = afe_command.afe_gpio_command.gpio_pin_direction;
NAFE_gpioConfig(afe_ctx->NafeHdl);
NAFE_gpioWriteData(afe_ctx->NafeHdl, afe_command.afe_gpio_command.gpio_output_value);
```

---

#### di9–di16  →  Not wired (no hardware effect)

These CoE subindices exist in the object dictionary but are **absent from
`appl_ecatPdoReceived()`**. The handler skips from di8 directly to di17.
Writing to 0x7000:0x09–0x10 from TwinCAT does nothing.

Note: on the **TxPDO side**, 0x6010:0x09–0x0F (di9–di15) *do* carry the MCU
digital input pin states — see below.

---

#### di17–di24  →  MCU RGPIO2 pins 21–28

[goal_appl_ecat.c:297-308](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L297-L308)

```c
uint8_t *pii = &riop_command.mcu_digital_output_command.mcu_digital_output;
#define ATOMIC_OP(var, XXX) if(var) *pii|=(uint8_t)XXX; else *pii&=~(uint8_t)XXX
ATOMIC_OP(di17_in, 0x01);   // bit 0
ATOMIC_OP(di18_in, 0x02);   // bit 1
ATOMIC_OP(di19_in, 0x04);   // bit 2
ATOMIC_OP(di20_in, 0x08);   // bit 3
ATOMIC_OP(di21_in, 0x10);   // bit 4
ATOMIC_OP(di22_in, 0x20);   // bit 5
ATOMIC_OP(di23_in, 0x40);   // bit 6
ATOMIC_OP(di24_in, 0x80);   // bit 7
```

Dispatched via `gpio_command_queue` → `GPIO_ControlTask`
([gpio_task.c:88-102](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c#L88-L102)):

```c
if(xQueueReceive(gpio_command_queue, &mcu_digital_output_command, 0) == pdPASS)
{
    for(uint8_t i = 0; i < MAX_MCU_DIG_GPIO_OUTPUT_PINS; i++)  // MAX = 8
    {
        RGPIO_PinWrite(RGPIO2, i+21, (mcu_digital_output_command >> i) & 1);
    }
    RIOP_SetMcuDigitalOutputPinsStatus(mcu_digital_output_command);
}
```

Physical pins from [pin_mux.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/board/pin_mux.h)
and [board.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/board/board.h):

| PDO object | C variable | Bit | RGPIO2 pin | Pad name           | IC coord |
|------------|------------|-----|------------|--------------------|----------|
| 0x7000:11  | di17_in    | 0   | 21         | GPIO_EMC_B1_21 (DIGITAL_OUTPUT_01) | K4 |
| 0x7000:12  | di18_in    | 1   | 22         | GPIO_EMC_B1_22 (DIGITAL_OUTPUT_02) | L3 |
| 0x7000:13  | di19_in    | 2   | 23         | GPIO_EMC_B1_23 (DIGITAL_OUTPUT_03) | F1 |
| 0x7000:14  | di20_in    | 3   | 24         | GPIO_EMC_B1_24 (DIGITAL_OUTPUT_04) | P2 |
| 0x7000:15  | di21_in    | 4   | 25         | GPIO_EMC_B1_25 (DIGITAL_OUTPUT_05) | N3 |
| 0x7000:16  | di22_in    | 5   | 26         | GPIO_EMC_B1_26 (DIGITAL_OUTPUT_06) | N4 |
| 0x7000:17  | di23_in    | 6   | 27         | GPIO_EMC_B1_27 (DIGITAL_OUTPUT_07) | J3 |
| 0x7000:18  | di24_in    | 7   | 28         | GPIO_EMC_B1_28 (DIGITAL_OUTPUT_08) | F5 |

---

#### di25–di32  →  NAFE13388 GPIO pin direction (GP2–GP9)

[goal_appl_ecat.c:310-321](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L310-L321)

```c
// Writes riop_command.afe_command.afe_gpio_command.gpio_pin_direction
ATOMIC_OP(di25_in, 0x0004);   // bit 2 → NAFE GP2 direction
ATOMIC_OP(di26_in, 0x0008);   // bit 3 → NAFE GP3 direction
ATOMIC_OP(di27_in, 0x0010);   // bit 4
ATOMIC_OP(di28_in, 0x0020);   // bit 5
ATOMIC_OP(di29_in, 0x0040);   // bit 6
ATOMIC_OP(di30_in, 0x0080);   // bit 7
ATOMIC_OP(di31_in, 0x0100);   // bit 8
ATOMIC_OP(di32_in, 0x0200);   // bit 9 → NAFE GP9 direction
```

A `1` bit configures that NAFE GP pin as an **output**; `0` = input.
Dispatched via `afe_command_queue` → `AFE_Task` → `NAFE_gpioConfig()`.

Initial direction at startup ([digital_io.c:75-76](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/digital_io.c#L75)):
```c
riop_command.afe_command.afe_gpio_command.gpio_pin_direction = 0x3C0;
// bits 6-9 set = GP6–GP9 as outputs; GP2–GP5 as inputs
```

---

#### 0x7000:21 (led_in)  →  LED_status / led_green

[goal_appl_ecat.c:323-330](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L323-L330)

```c
MEMCPY(&LED_status, &led_in, SIZEOF(LED_status));
led_green = LED_status;
```

`led_green` is checked in `appl_loop()`. When non-zero, the **green** LED
blinks; when zero, the **red** LED blinks.
Physical pin: RGPIO1 pin 15, pad GPIO_AON_15, coord B3
([pin_mux.h — LED_GREEN](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/board/pin_mux.h#L633)).

---

### 0x7010 — Analog Outputs (TwinCAT → firmware globals)

[goal_appl_ecat.c:325-353](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L325-L353)

| Sub  | C variable | Type   | Effect |
|------|------------|--------|--------|
| 0x01 | adc1_in    | UINT32 | Sets `fsm_mode` — selects AFE operating mode (0=auto-cycle, 1=HV, 2=current, 3=LV, 4=AFE GPIO, 5=MCU outputs, 6=temperature) |
| 0x02 | adc2_in    | UINT32 | Copied to local `tt`, discarded |
| 0x03 | adc3_in    | INT32  | Discarded |
| 0x04 | adc4_in    | INT32  | Discarded |
| 0x05–0x10 | adc5–16_in | FLOAT | Discarded |

---

## Inputs: Hardware → TwinCAT  (TxPDO, object 0x6010 / 0x6020)

Handled in `appl_ecatPdoTxPrepare()`:
[goal_appl_ecat.c:366](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L366)

### 0x6010 — Digital Inputs (UINT8 each)

#### di1–di8  ←  NAFE13388 GPIO input pins GP2–GP9

[goal_appl_ecat.c:378-390](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L378-L390)

```c
uint16_t *pi = &g_boardStatus.afe_status.afe_gpio_input_pins_status;
#define ATOMIC_OP(var, XXX) if(*pi&(uint16_t)XXX) var=1; else var=0
ATOMIC_OP(di1_out, 0x0004);   // bit 2 ← NAFE GP2
ATOMIC_OP(di2_out, 0x0008);   // bit 3 ← NAFE GP3
ATOMIC_OP(di3_out, 0x0010);   // bit 4 ← NAFE GP4
ATOMIC_OP(di4_out, 0x0020);   // bit 5 ← NAFE GP5
ATOMIC_OP(di5_out, 0x0040);   // bit 6 ← NAFE GP6
ATOMIC_OP(di6_out, 0x0080);   // bit 7 ← NAFE GP7
ATOMIC_OP(di7_out, 0x0100);   // bit 8 ← NAFE GP8
ATOMIC_OP(di8_out, 0x0200);   // bit 9 ← NAFE GP9
```

`afe_gpio_input_pins_status` is populated by `GPIO_ControlTask` calling
`RIOP_SetAfeDigitalInputPinsStatus(NAFE_gpioReadData(afe_ctx->NafeHdl))`.
These are the same GP2–GP9 pins as the di1–di8 outputs; their direction
(input vs output) is controlled by di25–di32.

---

#### di9–di15  ←  MCU RGPIO6 digital input pins

[goal_appl_ecat.c:392-401](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L392-L401)

```c
pii = &g_boardStatus.mcu_digital_input_pins_status;
#define ATOMIC_OP(var, XXX) if(*pii&(uint8_t)XXX) var=1; else var=0
ATOMIC_OP(di9_out,  0x01);   // bit 0 ← BOARD_DIG_IN1
ATOMIC_OP(di10_out, 0x02);   // bit 1 ← BOARD_DIG_IN2
ATOMIC_OP(di11_out, 0x04);   // bit 2 ← BOARD_DIG_IN3
ATOMIC_OP(di12_out, 0x08);   // bit 3 ← BOARD_DIG_IN4
ATOMIC_OP(di13_out, 0x10);   // bit 4 ← BOARD_DIG_IN5
ATOMIC_OP(di14_out, 0x20);   // bit 5 ← BOARD_DIG_IN6
ATOMIC_OP(di15_out, 0x40);   // bit 6 ← BOARD_DIG_IN7
```

`mcu_digital_input_pins_status` is packed by `GPIO_ControlTask`
([gpio_task.c:56-71](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c#L56-L71)):

```c
mcu_digital_input_pin[0] = RGPIO_PinRead(BOARD_DIG_IN1_GPIO, BOARD_DIG_IN1_GPIO_PIN);
// ... [1]–[6] for DIG_IN2–7
for(int8_t i = 6; i >= 0; i--)
    mcu_digital_input_status |= (mcu_digital_input_pin[i] << i);
RIOP_SetMcuDigitalInputPinsStatus(mcu_digital_input_status);
```

Physical pins from [pin_mux.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/board/pin_mux.h)
(all RGPIO6):

| PDO object | C variable | Bit | RGPIO6 pin | Pad name           | IC coord |
|------------|------------|-----|------------|--------------------|----------|
| 0x6010:09  | di9_out    | 0   | 12         | GPIO_B1_12 (DIGITAL_INPUT_01) | A11 |
| 0x6010:0A  | di10_out   | 1   | 8          | GPIO_B1_08 (DIGITAL_INPUT_02) | B14 |
| 0x6010:0B  | di11_out   | 2   | 7          | GPIO_B1_07 (DIGITAL_INPUT_03) | D13 |
| 0x6010:0C  | di12_out   | 3   | 13         | GPIO_B1_13 (DIGITAL_INPUT_04) | B11 |
| 0x6010:0D  | di13_out   | 4   | 6          | GPIO_B1_06 (DIGITAL_INPUT_05) | B10 |
| 0x6010:0E  | di14_out   | 5   | 9          | GPIO_B1_09 (DIGITAL_INPUT_06) | A15 |
| 0x6010:0F  | di15_out   | 6   | 10         | GPIO_B1_10 (DIGITAL_INPUT_07) | A13 |

---

#### di16  ←  Fixed 0

[goal_appl_ecat.c:402](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L402)

```c
uint8_t di = 0;
MEMCPY(&di16_out, &di, SIZEOF(di));   // always zero
```

Only 7 MCU digital inputs exist (`MAX_MCU_DIG_GPIO_INPUT_PINS = 7` in
[riop_feature_config.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/riop_feature_config.h#L14)),
so di16 has no corresponding pin.

---

#### di17–di24  ←  MCU digital output readback (bits 0–7)

[goal_appl_ecat.c:404-413](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L404-L413)

```c
pii = &g_boardStatus.mcu_digital_output_pins_status;
ATOMIC_OP(di17_out, 0x01);
// ...
ATOMIC_OP(di24_out, 0x80);
```

Reports the last-commanded state of the same RGPIO2 pins 21–28 listed in the
di17–di24 outputs table above. `mcu_digital_output_pins_status` is written by
`RIOP_SetMcuDigitalOutputPinsStatus()` immediately after `GPIO_ControlTask`
drives the pins.

---

#### di25–di32  ←  Not populated (always 0)

Confirmed by source comment:
[goal_appl_ecat.c:416](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L416)
`//NA di25-32`

---

#### 0x6010:21 (led_out)  ←  LED_status readback

[goal_appl_ecat.c:419](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L419)

```c
MEMCPY(&led_out, &LED_status, SIZEOF(LED_status));
```

Echo of the value TwinCAT last wrote to `led_in` (0x7000:21).

---

### 0x6020 — Analog Inputs (ADC values → TwinCAT)

All sourced from `g_boardStatus.afe_status`, populated by `AFE_Task` via the
NAFE13388 over LPSPI1 (data-ready interrupt on GPIO_AD_15 / GPIO4 pin 15).

[goal_appl_ecat.c:420-464](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c#L420-L464)

| PDO object | C variable | Type   | Source in `g_boardStatus`                   | Signal |
|------------|------------|--------|---------------------------------------------|--------|
| 0x6020:01  | adc1_out   | UINT32 | `adc_1` global (test value, not from ADC)   | — |
| 0x6020:02  | adc2_out   | UINT32 | `adc_1` (duplicate)                         | — |
| 0x6020:03  | adc3_out   | INT32  | `-adc_1`                                    | — |
| 0x6020:04  | adc4_out   | INT32  | `-adc_1`                                    | — |
| 0x6020:05  | adc5_out   | FLOAT  | `afe_status.hvsig_status.channel_value[0]`  | NAFE HV ext ch 0 |
| 0x6020:06  | adc6_out   | FLOAT  | `afe_status.hvsig_status.channel_value[1]`  | NAFE HV ext ch 1 |
| 0x6020:07  | adc7_out   | FLOAT  | `afe_status.hvsig_status.channel_value[2]`  | NAFE HV ext ch 2 |
| 0x6020:08  | adc8_out   | FLOAT  | `afe_status.current_status.channel_value`   | NAFE current ch  |
| 0x6020:09  | adc9_out   | FLOAT  | `afe_status.lvsig_status.channel_mean[0]`   | NAFE LV int ch 0 |
| 0x6020:0A  | adc10_out  | FLOAT  | `afe_status.lvsig_status.channel_mean[1]`   | NAFE LV int ch 1 |
| 0x6020:0B  | adc11_out  | FLOAT  | `afe_status.lvsig_status.channel_mean[2]`   | NAFE LV int ch 2 |
| 0x6020:0C  | adc12_out  | FLOAT  | `afe_status.lvsig_status.channel_mean[3]`   | NAFE LV int ch 3 |
| 0x6020:0D  | adc13_out  | FLOAT  | `afe_status.lvsig_status.channel_mean[4]`   | NAFE LV int ch 4 |
| 0x6020:0E  | adc14_out  | FLOAT  | `afe_status.temperature_status.channel_value` | NAFE temperature |
| 0x6020:0F  | adc15_out  | FLOAT  | `afe_status.temperature_status.channel_value` (repeat) | NAFE temperature |
| 0x6020:10  | adc16_out  | FLOAT  | `afe_status.temperature_status.channel_value` (repeat) | NAFE temperature |

---

## Update Rates

| Path | Period |
|------|--------|
| EtherCAT PDO cycle | Set by TwinCAT master (min 200 µs per `APPL_ECAT_MIN_CYCLE_TIME`) |
| `appl_loop()` dispatch to queues | Up to 200 ms (mode 0 default) or 20 ms (modes 4/5) per DWT timer in [digital_io.c:117](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/digital_io.c#L117) |
| `GPIO_ControlTask` poll | 50 ms per [gpio_task.c:104](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c#L104) |
| NAFE13388 ADC sampling | Default 1000 µs (`AFE_SAMPLING_PERIOD_US_TSN` in [riop_feature_config.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/riop_feature_config.h#L30)) |

---

## Key Source Files

| File | Role |
|------|------|
| [goal_appl_ecat.c](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat.c) | PDO callbacks — glue between CoE variables and `riop_command` / `g_boardStatus` |
| [goal_appl_ecat_objects.c](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/appl/goal_ecat/riop/goal_appl_ecat_objects.c) | CoE object dictionary and global PDO variable declarations |
| [digital_io.c](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/digital_io.c) | Defines `riop_command` global; `appl_loop()` dispatch loop; `fsm_mode` handling |
| [icc_task.c](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/icc_task/icc_task.c) | `ICC_DataHandleTask()` — dispatches `riop_command` to FreeRTOS queues; defines `g_boardStatus` |
| [gpio_task.c](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/gpio_task/gpio_task.c) | MCU GPIO read/write loop (50 ms) |
| [afe_task.c](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/afe_task/afe_task.c) | NAFE13388 ADC and GPIO over SPI |
| [api_riop.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/api_riop.h) | `riop_board_status_t` and `riop_command_t` structure definitions |
| [riop_feature_config.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/source/riop_feature_config.h) | Pin count constants and AFE timing |
| [pin_mux.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/board/pin_mux.h) | Physical pad → peripheral/GPIO mappings (MCUXpresso Config Tools generated) |
| [board.h](rd-riop-ecat-1.5.0/riop_ECAT/M7FOLLOWER/board/board.h) | `BOARD_DIG_IN*` / `BOARD_DIG_OUT*` macro aliases |
