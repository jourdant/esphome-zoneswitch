# Contributing

Start with the [architecture](docs/architecture.md), the relevant
[protocol specification](docs/specs/protocols/README.md), and [test instructions](tests/README.md).
V2 is confirmed fully working on V2.1 hardware. Preserve its default selection
and wire behavior when adding support for older or newly identified boards.

## Development

Use Python tooling through `uv`. Work on a focused branch and keep changes
reviewable. The component is under `esphome/components/zoneswitch`; entities
must not implement protocol framing or transceiver direction control.
Keep C++ compatible with ESPHome, format with the repository's `.clang-format`,
and format Python with Ruff. Run the relevant named host tests and actual
ESPHome schema/build checks described in `tests/README.md`.

For protocol changes, add a failing capture/transaction case first when possible.
Keep observed bytes, user-described physical behavior and your interpretation
separate. Add original logs to [research](docs/research/README.md), normalized
records to [fixtures](tests/fixtures/README.md), and update the matching spec.
Host tests do not substitute for on-wire timing or wall-controller coexistence
checks. Record exactly which firmware commit was tested on hardware.

Update the revision guide when installation requirements change, and the
configuration reference when adding/removing options. Never commit device
credentials, private YAML, or ESPHome build products. Public examples use
`!secret` references and clearly identify fragments versus complete devices.

## Reporting a problem or a new board

Use the issue templates. Include PCB marking, selected protocol, adapter and
GPIOs, baud/framing, ESPHome/framework version, component ref, sanitized YAML,
logs, and whether physical buttons/LEDs still work. Distinguish controller-reported
state from actual airflow. For unknown hardware, begin with passive captures.

## Review and releases

A review should explain the concrete behavior change and validation evidence,
including remaining hardware limitations. Add a concise entry to
[CHANGELOG.md](CHANGELOG.md). Follow the [release process](docs/releases.md) for
immutable tags and exact tested-version records. Do not promote V1 to fully
supported until its wall-panel coexistence issue is resolved and tested.

No license is added by this change. The owner must choose the license before a
license file or licensing claim is introduced.
