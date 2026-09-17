# Component architecture

The public `zoneswitch` YAML and entity platforms remain shared. At code generation,
`protocol` selects a concrete `V1ZoneSwitch` or `V2ZoneSwitch`, both derived from
`ZoneSwitch`. A hub allocates only the selected protocol's state. The parsers use fixed buffers, with no
per-packet buffer allocation or runtime protocol switch.

```text
ESPHome YAML validation → V1ZoneSwitch or V2ZoneSwitch
                          └── ZoneSwitch shared hub
                              ├── state listeners → switches / binary sensors
                              └── diagnostics → sensors / text sensors
V1ZoneSwitch → transport_v1 → ESP-IDF FIFO + RX ring buffer
V2ZoneSwitch → ESPHome UART → transceiver
```

| Boundary | Responsibility |
|---|---|
| `__init__.py` | Protocol defaults, transport compatibility, code generation |
| `zoneswitch.h/.cpp` | Entities, command admission, listen-only guard, spill guard, state age and diagnostics |
| `protocol_v1.h/.cpp` | Indexed status parser, physical prefix handling, query-before-toggle, transaction cancellation |
| `transport_v1.h/.cpp` | ESP32-S3 FIFO, bounded byte TX, immediate DE release and bounded ACK wait |
| `protocol_v2.h/.cpp` | CRC, frame parser, sequence/node learning, polling, persistence and response handling |
| Entity subdirectories | Home Assistant presentation; no protocol or UART logic |

V1 transport returns a typed outcome. Timing-critical code has no logging between
ACK reception and command transmission. Host tests replace that boundary, while
firmware builds compile the real transport. Host tests cannot prove DE timing.
V2 retains its established wire format and UART direction handling.

## Invariants

- Listen-only blocks boot, periodic, refresh and control transmissions. Receive parsing continues.
- Home Assistant state comes from accepted frames; commands do not optimistically publish state.
- V1 consumes intent before a toggle, rejects concurrent requests and never blindly retries an uncertain toggle.
- V2 waits for fresh state after a missed response; a mismatched sequence cannot acknowledge a pending write.
- Restoring a V2 address supplies an untrusted candidate; fresh confirmations still gate control writes.
- Receive work per loop is bounded. V1 handshake waits are bounded, with interrupts enabled during ACK waiting.
- Time comparisons use unsigned elapsed time or wrap-safe deadlines.

The shared UART and direction output must have a single transmitter owner. An
external bridge or arbitrary `uart.write` bypasses these component invariants.

See [tests](../tests/README.md), [protocol specs](specs/protocols/README.md) and
[remaining work](backlog.md).
