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
- Draft protocol spec created and updated with validated request/response semantics.
- Touchpad2 emulation plan created (staged passive -> active approach).
- External ESPHome component scaffolded (`zoneswitch`) with assignable zone entities.
- Read-only ESPHome example created for zone mask decoding.
- Azure Document Intelligence script added for reusable PDF/image OCR to Markdown.

## Key files

- Research captures and OCR output:
  - `docs/research/saved_rs485_packets.md`
  - `docs/research/saved_rs485_packets2.md`
  - `docs/research/ZoneSwitchV2_OpInstallationManual2015_12x17.md`
  - `docs/research/screenshot_ocr.md`
- Reverse-engineering outputs:
  - `docs/specs/polyaire_zoneswitch_protocol_spec_draft.md`
  - `docs/specs/esphome_zoneswitch_touchpad2_plan.md`
- ESPHome starter templates:
  - `esphome/esphome_zoneswitch_example_readonly.yaml`
  - `esphome/esphome_zoneswitch_example_readwrite.yaml`
  - `esphome/esphome_zoneswitch_component_example.yaml`
- External component source:
  - `esphome/components/zoneswitch/`
- OCR tooling:
  - `tools/azure_docint_to_markdown.py`
  - `tools/README.md`

## Protocol notes (snapshot)

From current captures:

- Fixed 9-byte framing with `AA` start and `55` end
- Request/response pair structure is consistent
- Sequence byte is mirrored in response
- Response payload includes a validated 6-bit zone mask
- Checksum is validated as CRC-8/MAXIM over bytes `[1..6]`

See the draft spec for exact byte-level detail and confidence labels.

The `esphome/esphome_zoneswitch_example_readwrite.yaml` file includes passive
decode and write controls with protocol-valid framing and checksum, but should
still be treated as experimental pending broader live edge-case validation.

## ESPHome external component usage

Use the new component to assign one or more zones and publish them as entities:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/jourdant/esphome-zoneswitch
      ref: main
    components: [ "zoneswitch" ]

uart:
  - id: zoneswitch_uart
    rx_pin: GPIO03
    tx_pin: GPIO04
    baud_rate: 9600

zoneswitch:
  - id: zs_bus
    uart_id: zoneswitch_uart

binary_sensor:
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    zone: 1
    name: "Zone 1 Active"

  - platform: zoneswitch
    zoneswitch_id: zs_bus
    zone: 2
    name: "Zone 2 Active"
```

For a complete example with all 6 zones, see
`esphome/esphome_zoneswitch_component_example.yaml`.

## Next recommended capture set

To unlock write control (zone on/off), collect button-action captures:

- Press each zone once (then again)
- Long-press spill-zone setting action
- Touchpad-off combo (Z3 + Z4)
- Capture with spill DIP OFF and ON

These traces should allow command opcode and checksum resolution.
