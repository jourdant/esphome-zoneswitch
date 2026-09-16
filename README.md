# ESPHome ZoneSwitch — V1 and V2

Control Polyaire ZoneSwitch zones through an RS485 transceiver and expose
controller-reported states to Home Assistant. Choose the protocol from the
wall PCB revision: **V2 is the default**; **V1 is experimental**.

| | V1 | V2 (default) |
|---|---|---|
| Observed wall PCB | V1.0-T | V2.1 |
| UART | 250000 baud, 8N1 | 9600 baud, 8N1 |
| Exchange | C0 → ACK 30 → toggle mask; 8-byte status | AA…55, 9-byte frames, CRC-8/MAXIM |
| Transport | ESP32-S3 / ESP-IDF, component-managed DE | Existing ESPHome UART transport |
| Node address | None observed | Learned address, optional persisted candidate |
| Default polling | Off; one startup query | On, every 5 seconds |
| Wall coexistence | LEDs may remain stale; buttons may temporarily stop responding | Existing V2 implementation retained |

**V1 caveat:** bench commands changed controller-reported state, but external
queries alone could disturb the wall panel. Repeated physical presses or a panel
restart recovered it. This implementation does not fix that issue. HA state is
neither proof of damper movement nor a guarantee that wall LEDs match.

## Configure the component

Add the external source and one protocol-specific UART/hub configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/jourdant/esphome-zoneswitch
      ref: codex/protocol-v1-v2
    components: [zoneswitch]
```

Pin a tested commit instead of a moving branch for reproducible installations.
Use ESPHome 2026.8.2 or newer; this branch is validated against 2026.8.2.

### V2 — default

```yaml
uart:
  id: zoneswitch_uart
  tx_pin: GPIO17
  rx_pin: GPIO18
  baud_rate: 9600
  flow_control_pin: GPIO21

zoneswitch:
  id: zs_bus
  uart_id: zoneswitch_uart
  protocol: v2  # Optional; existing configurations default to V2.
```

GPIO numbers above are examples. See the [V2 configuration](esphome/examples/v2.yaml),
[V2 protocol](docs/protocols/v2/protocol.md) and
[V2 wiring photographs](docs/hardware/v2.md).

### V1 — XIAO ESP32-S3 carrier

```yaml
uart:
  id: zoneswitch_uart
  tx_pin: GPIO43
  rx_pin: GPIO44
  baud_rate: 250000
  data_bits: 8
  parity: NONE
  stop_bits: 1
  rx_full_threshold: 1
  # No UART flow_control_pin: the component owns direction.

zoneswitch:
  id: zs_bus
  uart_id: zoneswitch_uart
  protocol: v1
  flow_control_pin: GPIO4
  enable_polling: false
  status_timeout: 0s
```

V1 requires `esp32.variant: esp32s3` and `framework.type: esp-idf`.
GPIO4 must drive tied DE and /RE: HIGH transmits, LOW receives. Remove earlier
GPIO4 outputs, diagnostic scripts, UART buttons and TCP bridges. The component
must exclusively own this UART's TX. If UART debugging is enabled, set
`dummy_receiver: false`. Its output omits the direct-FIFO TX and consumed ACK;
`zoneswitch.debug: true` logs completed handshakes instead.

See the [V1 configuration](esphome/examples/v1.yaml),
[complete XIAO device YAML](esphome/examples/v1-xiao-all-in-one.yaml), and
[V1 protocol and limitations](docs/protocols/v1/protocol.md).

## Home Assistant entities

The same entities work with either protocol:

```yaml
switch:
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    zone: 2
    name: Zone 2

button:
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    name: Refresh zone state

sensor:
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    metric: rx_ok
    name: Valid frames
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    metric: rx_bad
    name: Rejected frames

binary_sensor:
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    metric: online
    name: ZoneSwitch online
```

Zones are 1–6; expose only installed zones. A `binary_sensor` with `zone: 2`
instead of `metric` provides read-only status. Switches publish received state,
not optimistic command success. V1 queries before an ON/OFF request, toggles only
if necessary, and abandons an unconfirmed transaction without automatic retry.
Commands received while a V1 command is in progress are rejected; issue them
again after it finishes. V1 has no transaction sequence field, so collision-free
response attribution on the shared bus remains unproven.

V2 additionally supports `sensor`/`text_sensor` with `metric: node_address`.
V1 has no observed node address; omit these diagnostics (their value stays zero).

## Configuration reference

| Option | Default | Meaning |
|---|---|---|
| `protocol` | `v2` | Explicit `v1` opt-in |
| `debug` | false | Component protocol logs; logger must permit DEBUG |
| `enable_polling` | V2 true / V1 false | Periodic state queries |
| `poll_interval` | 5s, minimum 500ms | Periodic query interval; V1 also enforces a 1s transaction gap |
| `tx_idle_guard` | 20ms | Receive quiet time required before TX |
| `flow_control_pin` | unset | Component-owned DE; required for V1, optional legacy V2 path |
| `status_timeout` | V2 30s / V1 0s | Age before offline; 0 disables age expiry for quiet event-driven buses |
| `diagnostic_update_interval` | 10s | Batch counter updates; online changes publish immediately |
| `spill_zone` | 0 | Optional last-open spill-zone guard; 1–6 only when installation is known |
| `tx_node_addr` | 0x48 | V2 fallback address; 0 requires passive discovery |
| `node_confirmations` | 3 | V2 matching statuses needed to lock address |
| `node_mismatch_threshold` | 5 | V2 mismatches before relearning |
| `restore_node` | false | V2 persisted candidate, revalidated before writes |
| `offline_miss_threshold` | 5 | V2 missed-response threshold |

With V1's default age timeout disabled, online means a valid status has been
observed and no subsequent active transaction failed. It is not continuous
connection monitoring. Failed V1 transactions immediately mark it offline.
The V2 address/miss-threshold settings do not participate in V1 control.
Never configure both UART and component direction control.

## Repository layout and validation

- `esphome/components/zoneswitch/`: shared hub/entities plus `protocol_v1.cpp` and `protocol_v2.cpp`.
- `esphome/examples/`: version-specific YAML fragments.
- `docs/protocols/v1/`, `docs/protocols/v2/`: protocol evidence and limitations.
- `docs/research/v1/`, `docs/research/v2/`: original captures.
- [Tests](tests/README.md): parser, transaction, schema and ESP32 build checks.
- [Tools](tools/README.md): V2 checksum analysis and document OCR.
- [Backlog](docs/backlog.md): remaining protocol work.

The original `esphome/esphome_zoneswitch_component_example.yaml` path remains as
a package include of the V2 example for compatibility.
