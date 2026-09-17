# Validation — 2026-09-16

- Host executable compiled the actual shared/V1/V2 sources and passed captured
  status, malformed/truncated input, no-op ON/OFF, missed-response cancellation,
  startup/polling, spill and V2 CRC/node-learning cases.
- ESPHome 2026.8.2 schema tests passed both protocol defaults, version-specific
  examples and seven rejected V1 transport/configuration cases.
- ESP32-S3 / ESP-IDF 5.5.5 firmware builds passed for V1 and implicit-default V2,
  with all entity platforms and the new refresh button.
- The merged XIAO sensor/LED/control configuration compiled with placeholder
  network credentials. That local build is not a deployable device image.
- The V2 checksum tool still found exact CRC-8/MAXIM matches after capture moves.
- Python lint, Markdown local-link checks and Git whitespace checks passed.

These are software checks. The V1 direct-FIFO handshake was previously tested
on the user's bench; integrated-component deployment and on-wire coexistence
remain distinct from that earlier evidence. Wall LED/physical-button limitations
remain open and are documented in the V1 protocol notes.

## Integrated V1 device check

Device configuration pinned to `6c4c09392835c565f029930ca48940582a9ade0b`.
ESPHome dashboard compiled and uploaded OTA; the device reconnected and passed
its boot-success interval. HA exposed zones 2–5, Refresh, diagnostics, onboard
LED and SHT40/OPT3001 readings. Startup produced one valid status and zero rejects.

| Local time | Integrated control | Controller-reported mask |
|---|---|---|
| 20:54:21 | Startup state | 12 (zones 2/5 ON) |
| 20:55:18.716 | Query before Zone 2 OFF | 12 |
| 20:55:19.744 | Toggle 02 for OFF | 10 (only zone 5 ON) |
| 20:55:38.686 | Query before Zone 2 ON | 10 |
| 20:55:39.703 | Toggle 02 for ON | 12 (original state restored) |

Each active query/toggle logged `TX C0 -> RX 30 -> TX MASK; DE LOW; sent=YES`.
The component published Zone 2 OFF then ON only after received status. Other zones
were left unchanged. This confirms one integrated control cycle, not physical
LED synchronisation, damper movement, or long-term reliability. No periodic
polling was enabled. The prior wall-panel limitation remains unresolved.
