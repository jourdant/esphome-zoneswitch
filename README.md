# ZoneSwitch Reverse Engineering Workspace

Research and tooling for decoding the Polyaire ZoneSwitch V2 touchpad/main-module protocol and building an ESPHome integration.

## Project goals

- Decode RS485 packet format between ZoneSwitch touchpad and main control module
- Extract install/wiring details from vendor documentation and photos
- Build an ESPHome path to:
  - publish per-zone status (Z1..Z6)
  - eventually switch zones on/off
  - emulate a second touchpad on T2

## Current status

- Captured idle bus traffic is available and parsed.
- Draft protocol spec created with confidence levels and known gaps.
- Touchpad2 emulation plan created (staged passive -> active approach).
- Read-only ESPHome example created for zone mask decoding.
- Azure Document Intelligence script added for reusable PDF/image OCR to Markdown.

## Key files

- Research captures and OCR output:
  - `docs/research/zoneswitch/saved_rs485_packets.md`
  - `docs/research/zoneswitch/ZoneSwitchV2_OpInstallationManual2015_12x17.md`
  - `docs/research/zoneswitch/screenshot_ocr.md`
- Reverse-engineering outputs:
  - `docs/research/zoneswitch/polyaire_zoneswitch_protocol_spec_draft.md`
  - `docs/research/zoneswitch/esphome_zoneswitch_touchpad2_plan.md`
- ESPHome starter templates:
  - `esphome/esphome_zoneswitch_example_readonly.yaml`
  - `esphome/esphome_zoneswitch_example_readwrite.yaml`
- OCR tooling:
  - `tools/azure_docint_to_markdown.py`
  - `tools/README.md`

## Protocol notes (snapshot)

From current capture:

- Fixed 9-byte framing with `AA` start and `55` end
- Request/response pair structure is consistent
- Sequence byte is mirrored in response
- Response payload includes a likely 6-bit zone mask
- Checksum algorithm is still unresolved from idle-only data

See the draft spec for exact byte-level detail and confidence labels.

The `esphome/esphome_zoneswitch_example_readwrite.yaml` file includes both
passive decode and an initial transmit path for zone on/off. The transmit path
should be treated as experimental until command opcode/checksum are fully
validated with active button-press captures.

## Next recommended capture set

To unlock write control (zone on/off), collect button-action captures:

- Press each zone once (then again)
- Long-press spill-zone setting action
- Touchpad-off combo (Z3 + Z4)
- Capture with spill DIP OFF and ON

These traces should allow command opcode and checksum resolution.
