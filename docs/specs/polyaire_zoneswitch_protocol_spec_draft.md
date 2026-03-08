# Polyaire ZoneSwitch V2 Protocol Spec (Draft)

## Scope and confidence

This document combines:
- Installation manual OCR: `ZoneSwitchV2_OpInstallationManual2015_12x17.md`
- Diagram OCR: `screenshot_ocr.md`
- Bus capture: `saved_rs485_packets.md`

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

1) Poll-like request family:
- `AA 00 B4 SEQ 01 00 00 CHK 55`

2) Status-like response family:
- `AA B4 00 SEQ 81 01 0C CHK 55`

High-confidence interpretation:
- `DST/SRC` are swapped between request and response.
- `SEQ` is copied from request to response.
- `CMD` appears to be `0x01` (request) and `0x81` (response/ack).

## Timing and sequencing (confirmed)

- Observed cadence is approximately every 5 seconds at idle.
- Each poll request is followed by one immediate response with same sequence.
- Sequence observed from `0x66` to `0xC1` with one capture gap (`0x8E`..`0x96` not present), likely logging gap.

## Zone state encoding (high-confidence inference)

In the response family, byte `[6]` appears to carry zone-state bits.

Observed value in this capture:
- Response `[6] = 0x0C` (binary `0000_1100`)

Given capture context (“2 zones out of 6 were enabled”), this strongly suggests `[6]` is a **bitmask** with two set bits.

Most likely mapping (hypothesis to verify):
- bit0 -> Z1
- bit1 -> Z2
- bit2 -> Z3
- bit3 -> Z4
- bit4 -> Z5
- bit5 -> Z6

Under this mapping, `0x0C` means Z3 and Z4 ON.

## Checksum field (partially characterized)

- Byte `[7]` is a checksum/integrity byte (confirmed).
- It is **not** a simple XOR/sum/two’s-complement over obvious contiguous byte ranges.
- `CHK_response XOR CHK_request = 0x8E` for paired request/response frames in this capture (confirmed).

Current state:
- The checksum algorithm remains unresolved from passive idle capture alone.
- Active captures with known control actions are needed to solve it robustly.

## Practical protocol model for implementation

## Passive decode mode (implement now)

- Accept fixed 9-byte frames with `AA`/`55` markers.
- Validate frame family by bytes `[1,2,4,5]`.
- Use response `[6]` as zone bitmask candidate.
- Track current zone states and expose 6 zone entities.

This can be implemented immediately in ESPHome as read-only status.

## Active control mode (requires additional reverse-engineering)

To command zone on/off from ESPHome, these unknowns must be solved:
- Outbound command opcode(s) for button-press/zone-set semantics.
- Outbound payload semantics (zone index, desired state vs toggle).
- Exact checksum algorithm.

## Touchpad2 emulation approach (high-confidence design)

Goal: plug ESPHome hardware into **T2** and emulate a second touchpad.

Recommended staged approach:

1. **Passive parallel sniff** on T2 traffic first.
2. Confirm idle poll/status identical whether 1 or 2 physical touchpads are connected.
3. Capture traffic while pressing each zone on real touchpad (single press, long press, combos).
4. Derive write command(s) and checksum.
5. Enable ESPHome TX with collision-avoidance timing.

Electrical guidance:
- Use RS485 transceiver with receiver always enabled.
- Keep TX disabled by default; enable only for short command burst windows.
- Share ground reference with controller side.
- Validate line polarity (A/B) and idle bias before enabling TX.

## Minimal experiment matrix to finish the spec

Capture each case with timestamps and frame hex:
- Baseline idle (already done)
- Press Z1 once, wait 10 s
- Press same zone again
- Repeat for Z2..Z6
- Trigger touchpad off combo (Z3+Z4 >2 s)
- Trigger spill-zone set action (hold 5 s)
- Repeat with spill DIP OFF vs ON

Expected result:
- Isolate command family and payload encoding
- Solve checksum by fitting across mixed command data
- Confirm bit-to-zone mapping

## ESPHome deliverable split

- **Phase 1**: read-only status integration (zone bitmask decode)
- **Phase 2**: write support (zone switch entities)
- **Phase 3**: full Touchpad2 emulation behavior parity (combos/LED behavior)
