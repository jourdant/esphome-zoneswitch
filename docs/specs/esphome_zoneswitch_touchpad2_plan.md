# ESPHome Plan: ZoneSwitch Touchpad2 Emulation

## Objective

Create an ESPHome node on Touchpad port T2 that:
- Reads and publishes status for Z1..Z6
- Allows zone switching from Home Assistant
- Behaves like a second touchpad without breaking native touchpad operation

## What can be implemented immediately

From current decode confidence, implement now:
- UART frame parser for fixed 9-byte frames
- Response family recognition: `AA B4 00 SEQ 81 01 MASK CHK 55`
- `MASK` -> 6 binary sensors for zone state

This does not require checksum generation because it is passive/read-only.

## What still needs reverse-engineering

For write control, unknowns are:
- Command opcode for zone control
- Payload format (zone/state/toggle)
- Checksum algorithm

Until those are known, write entities should be disabled or marked experimental.

## Recommended ESPHome architecture

## UART settings

Use same UART settings as your known-good capture config. Keep transport half-duplex at RS485 transceiver level.

## Component behavior

1. `on_frame(frame)`:
   - Validate `len == 9`, `frame[0] == 0xAA`, `frame[8] == 0x55`
   - If `frame[1]==0xB4 && frame[2]==0x00 && frame[4]==0x81 && frame[5]==0x01`:
     - `zone_mask = frame[6]`
     - Publish zone states from bits 0..5

2. `request_status()`:
   - Optional: send poll frame once checksum is solved
   - Before checksum solve, rely on native bus traffic

3. `set_zone(zone, on)`:
   - Queue outbound command once command format/checksum solved

## Proposed entity model

- Binary sensors:
  - `zoneswitch_zone_1_state` ... `zoneswitch_zone_6_state`
- Diagnostic sensors:
  - `zoneswitch_last_seq`
  - `zoneswitch_last_raw_mask`
  - `zoneswitch_rx_frame_count`
  - `zoneswitch_rx_error_count`
- Future switches:
  - `zoneswitch_zone_1` ... `zoneswitch_zone_6`

## Safety and coexistence rules

- Do not transmit until command encoding is validated.
- Keep one authoritative poller on the bus to avoid collisions.
- If enabling TX, rate-limit writes and enforce inter-frame gap.
- Preserve spill-function semantics; do not force all zones closed when spill is enabled.

## Wiring notes for T2 emulation

- T1/T2 are documented as equivalent touchpad ports.
- Use RJ12 straight-through cable style required by ZoneSwitch.
- Respect maximum cable length for data cable (left-latched) from manual.
- Verify A/B polarity and ground reference before connecting transceiver TX.

## Completion checklist

- [ ] Validate bit-to-zone mapping with one-zone-at-a-time button tests
- [ ] Capture write command(s) for zone toggle
- [ ] Solve checksum algorithm from mixed traffic
- [ ] Implement ESPHome write path and HA switches
- [ ] Validate coexistence with physical touchpad on T1
- [ ] Validate spill-mode edge cases
