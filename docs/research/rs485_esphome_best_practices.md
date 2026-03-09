# ESPHome RS485 Component Best Practices Research

## Sources reviewed

- ESPHome UART docs
- ESPHome Modbus docs
- ESPHome Modbus Controller docs
- ESPHome External Components docs
- ESPHome developer component architecture docs
- Reference project style from `esphome-lgap`

## Distilled best practices

1. Treat UART transport and protocol parsing as separate concerns
   - Keep frame parsing/checksum/state machine in C++ parent component
   - Keep entity exposure in child platforms (`binary_sensor`, `switch`, etc.)

2. Use runtime-safe half-duplex TX handling
   - If RS485 transceiver requires DE/RE, gate TX via `flow_control_pin`
   - Enable TX only during write, then immediately disable

3. Validate framing before publishing entities
   - Confirm SOF/EOF, expected length, and checksum
   - Reject and count malformed frames

4. Prefer event-driven publishing over polling entities
   - Publish state on valid frame arrival
   - Avoid unnecessary periodic `component.update` loops for child entities

5. Make bus timing configurable
   - Poll interval should be configurable
   - Keep conservative default values to reduce collisions

6. Debounce write intents at protocol layer
   - Queue desired state changes
   - Apply at protocol cadence (poll cycle) rather than sending each UI click instantly

7. Support dynamic addressing when protocol allows
   - Learn session/node address from validated responses
   - Allow configurable fallback node for bootstrap cases

8. Child entities should be explicit and stable
   - Require explicit zone numbers and IDs in YAML for HA stability
   - Use clear names and icons in examples

9. Keep codegen schemas strict
   - Validate zone range, byte-range settings, and optional pins
   - Keep component options discoverable in docs/examples

10. Instrument minimal diagnostics
   - Track rx_ok/rx_bad counters and key debug logs
   - Keep verbose logging optional

## Comparison against this repo

### Already implemented

- Parent/child component architecture (`zoneswitch` + child platforms)
- Frame validation (SOF/EOF + checksum)
- CRC-8/MAXIM checksum verification
- Event-driven entity publishing on frame receive
- Dynamic node learning from responses
- Configurable polling and debug flags
- Optional flow control pin

### Improvements applied in this pass

- Added switch child platform (`platform: zoneswitch`) for per-zone control
- Added desired-state queueing and poll-cycle application (debounce)
- Added configurable `tx_node_addr` fallback and `enable_polling`
- Added explicit switch IDs + damper/vent icon usage in example YAML

### Remaining optional improvements

- Add configurable write backoff / retry limit
- Add dedicated diagnostic sensor platform for rx counters and node address
- Add explicit startup write-lock until first valid status frame (partially covered by desired-state handling)
