# Changelog

Project release versions are distinct from V1/V2 wire protocols and PCB revisions.

## Unreleased

- Identify V1.0-T and V2.1 from supplied PCB photographs, then follow revision-specific guides.
- Mark V2 as confirmed fully working on V2.1 hardware; preserve it as the default protocol.
- Separate V1 and V2 driver state behind the shared hub and isolate the V1 ESP-IDF transport.
- Add `listen_only`, status age, transaction result, TX count, response timeout,
  V1 ACK timeout and busy-rejection diagnostics.
- Reject V2-only options/node diagnostics in V1 configurations instead of silently ignoring them.
- Organize specifications, source evidence and capture replay fixtures; expand named regression tests.
- Add contribution guidance, issue templates and a release process. No CI workflow is introduced.

### Migration

Existing V2 configuration remains supported. For V1, remove `tx_node_addr`,
`node_confirmations`, `node_mismatch_threshold`, `restore_node`,
`offline_miss_threshold`, and `node_address` diagnostics. These were inapplicable
to V1 and are now rejected, including explicit default values.

`enable_polling: false` retains its existing meaning. To suppress **all**
component transmissions, explicitly set `listen_only: true` and remove/disable
any explicit `enable_polling: true`. See the
[configuration reference](docs/configuration.md).

## Development milestones

- `6c4c093` — initial V1/V2 component; ESPHome 2026.8.2 build and one integrated
  V1 query/OFF/ON cycle confirmed. Wall LED/button limitation remains unresolved.
- `67f1841` — remove deprecated runtime UART check; retain compile-time protocol baud/framing validation.

These commits are historical references, not release tags.
