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
