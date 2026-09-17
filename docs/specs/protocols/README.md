# Wire protocols

| Property | [V1](v1/protocol.md) | [V2](v2/protocol.md) |
|---|---|---|
| Identified wall PCB | V1.0-T (back) | V2.40 (front) / V2.1 (back) on Ashish's board; another V2.40 front confirmed |
| Support | Experimental; software control demonstrated | Confirmed fully working; default |
| UART | 250000, 8N1 | 9600, 8N1 |
| Request | TX C0, RX 30, TX mask | 9-byte AA…55 frame |
| State | 8 indexed/checksummed bytes | 9-byte frame; CRC-8/MAXIM |
| Correlation | No sequence observed | Sequence and learned node |
| Default activity | One startup query, then event driven | Poll every 5 seconds |
| Known limitation | External traffic can disturb wall LEDs/buttons | Additional response ARG0 variants remain unverified |

`listen_only: true` suppresses all component transmissions on either protocol.
The per-protocol specifications distinguish observed wire behavior from inference
and driver policy. An accepted frame reports controller state, not measured airflow.

## Evidence trail

- [Known compatible boards](../../hardware/compatibility.md): front/back markings, photos and contributor reports.
- [Research index](../../research/README.md): original V1/V2 captures and manuals.
- [Normalized replay fixtures](../../../tests/fixtures/README.md): bytes, expected masks and source lines.
- [Validation records](../../research/validation/2026-09-17.md): software checks versus hardware observations.
- [V2 historical touchpad investigation](v2/touchpad2-plan.md): retained research, not an installation requirement.
