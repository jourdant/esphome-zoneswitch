# ESPHome Plan: ZoneSwitch Touchpad2 Emulation

## Objective

Create an ESPHome node on Touchpad port T2 that:
- Reads and publishes status for Z1..Z6
- Allows zone switching from Home Assistant
- Behaves like a second touchpad without breaking native touchpad operation

## What can be implemented immediately

From current decode confidence, implement now:
- UART frame parser for fixed 9-byte frames
- Response family recognition: `AA NODE 00 SEQ 81 01 MASK CHK 55`
- `MASK` -> 6 binary sensors for zone state

This does not require checksum generation because it is passive/read-only.

## What still needs reverse-engineering

Now validated from `saved_rs485_packets2.md`:
- Request command family: `AA 00 NODE SEQ 01 00 ARG1 CHK 55`
- `ARG1` is a zone toggle bit (`0x01..0x20`), not a full desired mask write
- Response mask is live zone-state bitmask (`MASK`)
- `NODE` should be treated as session-scoped and learned dynamically from traffic
- Checksum is CRC-8/MAXIM over bytes `[1..6]` (`DST..ARG1/MASK`)

Further checked against `saved_rs485_packets3.md`:
- Component-generated idle polls for `NODE=0x48`, `SEQ=0x50..0x5D`, `ARG1=0x00` have valid CRC-8/MAXIM checksums.
- The log shows `RX OK` increasing while `ZoneSwitch Node Address` stays `0`, but this capture predates the auto-detection PR. With that change applied, the node address should now update from valid discovered traffic.
- Do not use the raw `RX OK` counter alone as proof that active polling is working. Use a learned non-zero node address, online status, and decoded mask updates as the success criteria.

Write entities can now be implemented with standard safety guards.

Current implementation safety updates:
- Zone switch commands are ignored until a locked status frame initializes the live mask.
- New switch intents are based on the latest `last_mask_` when no write is already pending, avoiding stale desired-mask diffs.
- The first discovered node is exposed for diagnostics, but the node/`ARG0` pair is not locked until `node_confirmations` matching status frames are seen.
- TX now requires a configurable UART idle window (`tx_idle_guard`) before transmitting.
- RS485 driver-enable is held for a conservative post-`flush()` margin.

## Recommended ESPHome architecture

## UART settings

Use same UART settings as your known-good capture config. Keep transport half-duplex at RS485 transceiver level.

## Component behavior

1. `on_frame(frame)`:
   - Validate `len == 9`, `frame[0] == 0xAA`, `frame[8] == 0x55`
   - If `frame[2]==0x00 && frame[4]==0x81`:
    - Track candidate `node_addr` and response-side `frame[5]`/`ARG0` values.
    - Lock the session only after `node_confirmations` matching status frames.
     - Current captures show `frame[5]==0x01` for status responses.
     - `node_addr = frame[1]`
     - `zone_mask = frame[6]`
     - Publish zone states from bits 0..5
   - Treat checksum-valid request frames as valid traffic only, not as status updates.

2. `request_status()`:
   - Optional: send poll frame using CRC-8/MAXIM
   - Passive mode can still rely on native bus traffic

3. `set_zone(zone, on)`:
   - If desired state differs from current state, send one toggle command (`ARG1 = zone_bit`)
   - Compute checksum with CRC-8/MAXIM over bytes `[1..6]`

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

- Keep transmit disabled until runtime `node_addr` is learned from valid incoming responses.
- Keep one authoritative poller on the bus to avoid collisions.
- If enabling TX, rate-limit writes and enforce inter-frame gap.
- Require a recent-RX idle window before each TX to reduce collision risk.
- Preserve spill-function semantics; do not force all zones closed when spill is enabled.
- When validating active TX, confirm that the node-address diagnostic changes from `0` to the learned controller node. If only `RX OK` increments, debug transceiver direction/echo/collision behavior before sending zone toggles.
- The `saved_rs485_packets3.md` node-address value of `0` should be treated as pre-auto-detection behavior, not a current protocol limitation.

## Edge cases and safeguards

1. Toggle-vs-state mismatch (highest priority)
  - Protocol writes are toggle events, not absolute set-state commands.
  - Safeguard: only transmit a toggle when desired state differs from current `zone_mask`.

2. Session-scoped node address
  - `NODE` can change between sessions.
  - Safeguard: expose the first discovered `node_addr` for diagnostics, but do not lock it for active control until several matching frames confirm it.

3. Bus collisions (physical touchpad + ESPHome)
  - Concurrent writes can corrupt or drop frames.
  - Safeguard: rate-limit writes, keep inter-frame gap, and re-check state after each write.

4. Spill interaction
  - Spill logic may force a zone open when others are closed.
  - Safeguard: treat spill-forced state as authoritative and avoid repeated counter-writes.

5. Startup uncertainty
  - On boot, state may be unknown.
  - Safeguard: block writes until first valid status frame initializes `zone_mask`.

6. Restoring discovered node address
  - A restored node could save discovery time, but captures show the node is session-scoped and may change.
  - Safeguard if implemented later: restore only as a fallback candidate, require fresh confirmations before writes, and invalidate the restored node after missed responses.

## Wiring notes for T2 emulation

- T1/T2 are documented as equivalent touchpad ports.
- Use RJ12 straight-through cable style required by ZoneSwitch.
- Respect maximum cable length for data cable (left-latched) from manual.
- Verify A/B polarity and ground reference before connecting transceiver TX.

## Completion checklist

- [x] Validate bit-to-zone mapping with one-zone-at-a-time button tests
- [x] Capture write command(s) for zone toggle
- [x] Solve checksum algorithm from mixed traffic
- [x] Validate component-generated idle poll checksums against the checksum model
- [x] Guard stale desired-mask writes
- [x] Require multiple node confirmations before locking autodetected address
- [x] Add TX idle-window guard
- [ ] Retest active poll with auto-detection and confirm node address updates
- [x] Implement ESPHome write path and HA switches
- [ ] Validate coexistence with physical touchpad on T1
- [ ] Validate spill-mode edge cases
