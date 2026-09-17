# Configuration examples

| File | Type | Intended use |
|---|---|---|
| [v1.yaml](v1.yaml) | Fragment | Merge UART, hub and entities into an ESP32-S3/ESP-IDF device |
| [v2.yaml](v2.yaml) | Fragment | Merge into a device and set adapter-specific GPIOs |
| [v1-xiao-all-in-one.yaml](v1-xiao-all-in-one.yaml) | Complete device | XIAO carrier, installed zones 2–5, onboard sensors and LED |
| [secrets.example.yaml](secrets.example.yaml) | Credential names | Copy locally to `secrets.yaml` and fill in your own values |

Choose a revision in the [main README](../../README.md) before installing.
Fragments do not contain the complete ESP32, network or API setup. Do not paste
a second `uart`, `sensor` or other duplicate YAML key; merge list entries into the
existing section. Examples follow `main`. Pin `external_components.source.ref`
to a tested commit or a published release tag for reproducible installations.

For passive commissioning set `listen_only: true` and disable explicitly enabled
polling. The [configuration reference](../../docs/configuration.md) explains why
`enable_polling: false` alone does not stop startup/manual transmissions.
The old `esphome_zoneswitch_component_example.yaml` path remains a V2 include.
