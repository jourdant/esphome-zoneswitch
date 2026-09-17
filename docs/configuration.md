# Configuration reference

[Choose your board and guide](../README.md#identify-your-wall-controller) first.

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
V1 has no observed node address; configuration validation rejects this metric for V1.

## Configuration reference

| Option | Default | Meaning |
|---|---|---|
| `protocol` | `v2` | Explicit `v1` opt-in |
| `listen_only` | false | Suppress every component transmission, including startup, refresh and control |
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
The V2 address/miss-threshold settings are rejected in V1 configurations, even when explicitly set to their V2 defaults.
Never configure both UART and component direction control.


## Passive commissioning

Set `listen_only: true` on the hub. Periodic polling then defaults off; explicitly
combining it with `enable_polling: true` is a configuration error. The component
holds its own direction output LOW and blocks startup queries, refresh requests
and zone writes. Received state and diagnostics still work. This only governs
ZoneSwitch: remove other UART writers, scripts and bridges from the device.

`enable_polling: false` alone is **not** listen-only: V1 still queries at startup,
and manual refresh and controls still transmit on either protocol.

## Optional diagnostics

Use these with the `sensor` platform unless noted. All counters reset on reboot.
`diagnostic_update_interval` limits routine publication; online changes and
transaction results publish immediately. `status_age` updates on that interval
while the bus is silent and is unknown until the first accepted status.

| Metric | Protocol | Meaning |
|---|---|---|
| `rx_ok` | Both | Structurally/checksum-valid frames; V2 includes requests and status candidates |
| `rx_bad` | Both | Invalid or incomplete frames rejected by the parser |
| `status_age` | Both | Seconds since the last accepted status; unrelated traffic does not reset it |
| `tx_count` | Both | V1 attempted handshakes; V2 transmitted requests; not a count of zone changes |
| `response_timeouts` | Both | Transactions that did not receive status before their deadline |
| `ack_timeouts` | V1 | Handshakes with no ACK received within 10 ms |
| `rejected_busy` | V1 | Zone requests refused because another command is in progress |
| `node_address` | V2 | Current candidate/locked address; sensor or text_sensor (hex/decimal) |
| `transaction_result` | Both, text_sensor | Most recent transaction/admission event, listed below |

```yaml
sensor:
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    metric: status_age
    name: ZoneSwitch status age
    unit_of_measurement: s
    accuracy_decimals: 1
    entity_category: diagnostic
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    metric: response_timeouts
    name: ZoneSwitch response timeouts
    accuracy_decimals: 0
    entity_category: diagnostic
text_sensor:
  - platform: zoneswitch
    zoneswitch_id: zs_bus
    metric: transaction_result
    name: ZoneSwitch transaction result
    entity_category: diagnostic
```

Results: `none`, `awaiting_status`, `status_received`, `ack_timeout`,
`ack_invalid`, `tx_failed`, `response_timeout`, `rejected_busy`, `listen_only`,
`no_status`, `invalid_zone`, `status_timeout`. The latter records state-age expiry,
which cancels any in-flight transaction. `status_received` means a valid reply was observed,
not that a physical damper moved or a V1 wall LED updated. An already-satisfied
V1 request ends after its query; it sends no toggle. A rejected request can briefly
replace the result of an in-progress transaction until that transaction completes.

## UART validation

Compilation rejects a baud rate inconsistent with `protocol`: V1 requires
250000 and V2 requires 9600. Both require 8 data bits, no parity and one stop bit.
This checks the configured UART; it does not measure the physical wire speed.
V1 additionally requires ESP32-S3/ESP-IDF, native UART, `rx_full_threshold: 1`,
non-inverted RX/TX and an internal non-inverted push-pull direction GPIO.
Only one of the UART and hub may own direction control. Set UART debug's
`dummy_receiver: false`; the component is the receiver.
