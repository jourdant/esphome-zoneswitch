# Polyaire ZoneSwitch V2 protocol

**Confirmed fully working on V2.1 hardware; the default protocol.**
The owner confirmed this support on 2026-09-17. Historical experiments below
remain evidence for individual fields; their earlier open tasks do not imply
that standard zone control is still unimplemented.

[Setup guide](../../../guides/v2.1.md) · [Research sources](../../../research/README.md)

## Scope and confidence

This document combines:

- Installation manual OCR: `ZoneSwitchV2_OpInstallationManual2015_12x17.md`
- Diagram OCR: `screenshot_ocr.md`
- Bus capture: `saved_rs485_packets.md`
- Bus capture: `saved_rs485_packets2.md`
- Component TX/debug capture: `saved_rs485_packets3.md`

Confidence legend:

- **Confirmed**: directly observed in manual or packet data
- **High-confidence inference**: strongly supported by repeated packet patterns
- **Hypothesis**: plausible, needs active testing

## System architecture (confirmed)

- Main control module supports up to **6 zones**.
- Up to **2 touchpads** can be connected simultaneously.
- Touchpad ports are **T1** and **T2**, and are documented as equivalent.
- Main module uses star wiring to damper motors.
- Cabling is RJ12 and **straight-through** for both data and control.
- Two cable styles are documented:
  - **Control cable (centre latched)** max length: **20 m**
  - **Data cable (left latched)** max length: **10 m**
- Electrical domain is 24 VAC (transformer output 24 VAC, 50 Hz).

## Touchpad behavior (confirmed)

- Zone buttons are ordered **Z1..Z6 from top to bottom** on the touchpad.
- Press toggles zone open/closed and LED indicates zone status.
- Pressing Z3 + Z4 (>2 s) turns touchpad LEDs off (zone state preserved).
- Spill function exists, default spill zone is Z2 when spill switch is enabled.

## Observed serial frame format (confirmed)

From the idle capture, all messages are fixed-length **9-byte frames**:

`[0] SOF [1] DST [2] SRC [3] SEQ [4] CMD [5] ARG0 [6] ARG1 [7] CHK [8] EOF`

Where:
- `SOF = 0xAA` (always)
- `EOF = 0x55` (always)
- Length is always 9 bytes in capture

### Directional frame families (confirmed)

Two frame families repeat in request/response pairs:

1) Request family:
- `AA 00 NODE SEQ 01 00 ARG1 CHK 55`

2) Status response family:
- `AA NODE 00 SEQ 81 01 MASK CHK 55`

Current packet captures show response `ARG0=0x01`. Implementations may still
learn and lock `ARG0` per session after validating `SRC=0x00` and `CMD=0x81`,
but additional captures are needed before documenting any other `ARG0` values as
confirmed protocol variants.

High-confidence interpretation:
- `DST/SRC` are swapped between request and response.
- `SEQ` is copied from request to response.
- `CMD` appears to be `0x01` (request) and `0x81` (response/ack).
- `NODE` is a bus node/session address byte and is not a reliable static hardware ID.
  - Capture set 1 used `NODE=0xB4`
  - Capture set 2 (same physical system, different session) used `NODE=0x48`
  - Practical implication: implementations should learn this address at runtime from responses.

### Component-generated poll trace (confirmed)

`saved_rs485_packets3.md` contains ESPHome component debug lines rather than raw
UART pairs. It shows repeated transmitted idle poll requests:

- `node=0x48`
- `seq=0x50..0x5D`
- `arg1=0x00`
- `chk=0xA4,0x2B,0xA3,0x2C,0xAA,0x25,0xAD,0x22,0xB8,0x37,0xBF,0x30,0xB6,0x39`

These frames exactly match the established request family that would appear on
the wire as:

`AA 00 48 SEQ 01 00 00 CHK 55`

The capture also shows `ZoneSwitch RX OK` increasing while `ZoneSwitch Node
Address` remains `0`. This was captured before the later auto-detection PR was
submitted. With that PR, the runtime node address should now be updated from
valid discovered traffic instead of remaining at `0` in this scenario.

Practical implication: `RX OK` should still be interpreted as “valid frame
checksum”, not “valid status response received”. However, after the
auto-detection change, a learned non-zero node address and/or an online status
flag should be expected once valid controller traffic is present.

Implementation note: to avoid locking onto a single accidental status-looking
frame, the component now exposes the first discovered candidate node for
diagnostics but only locks the node/`ARG0` pair after three matching status
frames by default.

## Timing and sequencing (confirmed)

- Observed cadence is approximately every 5 seconds at idle.
- Each poll request is followed by one immediate response with same sequence.
- Sequence observed from `0x66` to `0xC1` with one capture gap (`0x8E`..`0x96` not present), likely logging gap.
- Component-generated polls in `saved_rs485_packets3.md` also advance `SEQ` by 1
  per 5-second poll (`0x50..0x5D`).

## Zone state encoding (confirmed)

In the response family, byte `[6]` carries zone-state bits.

Observed value in this capture:
- Response `[6] = 0x0C` (binary `0000_1100`)

Given capture context (“2 zones out of 6 were enabled”), this strongly suggests `[6]` is a **bitmask** with two set bits.

Bit mapping (validated by sequential button presses in `saved_rs485_packets2.md`):
- bit0 -> Z1
- bit1 -> Z2
- bit2 -> Z3
- bit3 -> Z4
- bit4 -> Z5
- bit5 -> Z6

Under this mapping, `0x0C` means Z3 and Z4 ON.

Validation from the new capture:
- Starting from all-off (`MASK=0x00`), pressing Z1..Z6 yields masks:
  - `0x01`, `0x03`, `0x07`, `0x0F`, `0x1F`, `0x3F`
- Turning zones back off yields expected reverse masks.

This confirms response byte `[6]` is the live zone mask.

## Request ARG1 semantics (confirmed)

From `saved_rs485_packets2.md`, request byte `[6]` (`ARG1`) is a zone-bit value:
- `0x01`, `0x02`, `0x04`, `0x08`, `0x10`, `0x20` when a zone button is pressed
- `0x00` for idle/status poll

Interpretation: request `ARG1` is a **toggle event bit** (button-like semantics),
not a full desired mask write.

## Checksum field (confirmed)

- Byte `[7]` is a checksum/integrity byte.
- Algorithm is **CRC-8/MAXIM** (`poly=0x31`, `init=0x00`, `refin=true`, `refout=true`, `xorout=0x00`).
- Canonical input range is bytes `[1..6]` (`DST,SRC,SEQ,CMD,ARG0,ARG1/MASK`), i.e. excludes SOF/EOF/checksum bytes.
- This model matched all raw frames from both captures (`saved_rs485_packets.md` and `saved_rs485_packets2.md`) with 100% accuracy.
- It also matches all 14 component-generated poll checksums in `saved_rs485_packets3.md` for `AA 00 48 SEQ 01 00 00 CHK 55`.

## Current component behavior

The driver accepts fixed nine-byte frames, checks AA/55 markers and CRC-8/MAXIM,
and learns the node/response ARG0 pair from repeated matching status frames.
Three confirmations are required by default. A persisted address is an untrusted
candidate until fresh traffic confirms it. Status mask bits map to zones 1–6.

Absolute ON/OFF requests are translated to the required toggle bit relative to
received state. Writes are gated on valid online status. Missing write replies
require fresh state before another toggle; a status with a different sequence
can update passive state but does not acknowledge a pending write.

Polling defaults to five seconds. `listen_only: true` suppresses every component
transmission, including refresh and control, while reception and node learning
continue. See the [configuration reference](../../../configuration.md) for address,
status-age, miss-threshold, spill-zone and diagnostic settings.

The third saved capture predates node autodetection: its zero node diagnostic
is historical, not the expected current behavior after valid traffic is learned.
The V2 software refactor retains the established wire behavior; its software
checks and hardware evidence are recorded separately in
[validation](../../../research/validation/2026-09-17.md).

## Further evidence to collect

Response ARG0 values other than 0x01 and additional board revisions need their
own captures before compatibility is claimed. Button combinations, spill-zone
configuration actions and behavior with multiple physical touchpads are useful
additional traces. The original [touchpad investigation plan](touchpad2-plan.md)
is retained as historical research; it is not a prerequisite for standard V2 setup.
