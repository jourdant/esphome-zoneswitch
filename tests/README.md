# Validation

Run from the repository root:

```sh
c++ -std=c++17 -Itests/stubs -Iesphome/components/zoneswitch \
  tests/protocol_test.cpp esphome/components/zoneswitch/zoneswitch.cpp \
  esphome/components/zoneswitch/protocol_v1.cpp \
  esphome/components/zoneswitch/protocol_v2.cpp -o /tmp/zoneswitch-protocol-test
/tmp/zoneswitch-protocol-test
uv run --with esphome==2026.8.2 python -m unittest discover -s tests
uv run --with esphome==2026.8.2 esphome compile tests/v1.yaml
uv run --with esphome==2026.8.2 esphome compile tests/v2.yaml
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
