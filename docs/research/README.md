# Research and captures

This directory preserves source evidence. Protocol interpretations live in
[specs](../specs/protocols/README.md); installation steps live in the revision guides.
Historical notes may contain hypotheses superseded by later captures.

| Evidence | Contents |
|---|---|
| [V1 all six buttons](v1/2026-09-16_user-all-six-zones.md) | Physical ON/OFF sequence, timestamps, shared-bus UART bytes |
| [V1 startup](v1/2026-09-16_startup-captures.md) | Startup-style mask 00 and zone states |
| [V1 transport experiments](v1/2026-09-16_direct-fifo-results.md) | Working handshake, timing and wall-panel limitations |
| [V2 capture 1](v2/captures/saved_rs485_packets.md) | Idle request/status pairs |
| [V2 capture 2](v2/captures/saved_rs485_packets2.md) | Additional captured traffic |
| [V2 capture 3](v2/captures/saved_rs485_packets3.md) | UART and component TX logs |
| [Installation manual OCR](ZoneSwitchV2_OpInstallationManual2015_12x17.md) | Extracted manual text; verify against the [original PDF](ZoneSwitchV2_OpInstallationManual2015_12x17.pdf) |
| [Historical screenshot OCR](screenshot_ocr.md) | Extracted screenshot material |
| [RS485 research](rs485_esphome_best_practices.md) | Earlier UART/RS485 notes |
| [2026-09-16 validation](validation/2026-09-16.md) | Initial implementation and integrated V1 software control cycle |
| [2026-09-17 validation](validation/2026-09-17.md) | Refactor checks and V2 support confirmation |
| [Replay fixtures](../../tests/fixtures/README.md) | Machine-readable extracts linked back to original source lines |

## Add evidence

Store a capture under `v1/` or `v2/captures/`. Record PCB marking, adapter,
firmware/ESPHome version, baud/framing, GPIO and direction ownership, physical
button actions, initial/final LEDs, and whether the capture is passive or active.
Keep original bytes and timestamps. Label sender attribution as unknown when a
shared-bus trace cannot prove who transmitted. Remove credentials and identifiers
unrelated to the investigation. Explain observations separately from hypotheses.
