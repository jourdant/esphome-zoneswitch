# Validation

Run from the repository root:

```sh
c++ -std=c++17 -Itests/stubs -Iesphome/components/zoneswitch \
  tests/protocol_test.cpp esphome/components/zoneswitch/zoneswitch.cpp \
  esphome/components/zoneswitch/protocol_v1.cpp \
  esphome/components/zoneswitch/protocol_v2.cpp \
  esphome/components/zoneswitch/transport_v1.cpp -o /tmp/zoneswitch-protocol-test
/tmp/zoneswitch-protocol-test
uv run --with esphome==2026.9.0 python -m unittest discover -s tests
uv run --with esphome==2026.9.0 esphome compile tests/v1.yaml
uv run --with esphome==2026.9.0 esphome compile tests/v2.yaml
```

Use ESPHome's managed ESP-IDF environment. If your shell exports an unrelated
`IDF_PATH`/`IDF_PYTHON_ENV_PATH`, unset those for the compile command.
The two build fixtures have no network credentials and must not be installed on
a deployed device. They compile all entity types, V1 fast transport and the
implicit V2 default. The host executable compiles the actual hub/protocol sources
against minimal ESPHome stubs. Only the V1 hardware transaction is substituted;
parsing, state publication decisions, timeouts and command scheduling are real.

Coverage: captured six-zone states, physical prefixes (including zone-4 08),
fragmentation, bad checksums/indices, truncated-frame expiry, idempotent ON/OFF,
missed replies and no blind retry, startup query, disabled periodic polling,
spill guard, V2 CRC/address confirmation/request generation. Schema checks reject
wrong baud, wrong RX threshold, competing DE owners, dummy receiver, inverted DE,
and unsupported framework. These checks cannot validate on-wire timing,
coexistence with a real wall controller or damper movement.

The same schema/build matrix can be run with ESPHome 2026.8.2. Exact results,
not an open-ended "or newer" compatibility claim, belong in the
[validation record](../docs/research/validation/2026-09-17.md).

Named host groups cover V1 transactions, V2 CRC/control, both protocols in
listen-only mode (including direction pin staying LOW), busy/timeout diagnostics,
V2 mismatched sequences and node changes, clock wrap/reboot interruption, and
[recorded capture replay](fixtures/README.md). Failures name the case/expression.
Python checks validate fixture source bytes and reject incompatible options,
metrics, UART framing and polling in listen-only mode. They also validate the
complete carrier example with dummy credentials and both merge fragments.

No CI workflow or combined check runner is provided; run the relevant commands
above directly. Never upload the network-free compile fixtures to a live device.
