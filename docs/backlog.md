# ZoneSwitch Backlog

Open engineering and reverse-engineering tasks that are worth keeping visible,
but are not required for the current safe-control implementation.

## Protocol validation

### Generalize response `ARG0` handling in tooling and documentation

Current captures confirm status responses with `ARG0=0x01`:

`AA NODE 00 SEQ 81 01 MASK CHK 55`

The component can learn the response-side `ARG0` value dynamically, but the
checksum solver still classifies responses with a hard-coded `ARG0=0x01` filter.

Next steps:

- Capture startup/idle/toggle traffic after the node autodetection PR is applied.
- Update `tools/zoneswitch_checksum_solver.py` to classify response candidates as
  `SRC=0x00 && CMD=0x81`, then report observed `ARG0` values rather than filtering
  only `ARG0=0x01`.
- If another `ARG0` value appears, add the backing capture before documenting it
  as a confirmed protocol variant.
- Keep implementation comments aligned with capture-backed evidence.

## Node autodetection and startup latency

### Explore persisted node fallback with periodic reaffirmation

The component currently learns the session node from live traffic and locks it
after multiple matching status frames. This is conservative, but startup can be
slower if the bus is quiet or if active polling is disabled until discovery.

Possible strategy:

- Persist the last confirmed node address across reboots.
- On boot, restore it only as an untrusted fallback candidate, not as a locked
  node.
- Require fresh status confirmations before enabling zone writes.
- Periodically reaffirm the selected node while running, e.g. every 100th valid
  status frame or on a time interval.
- Build on the runtime `node_mismatch_threshold` guard: count valid-looking status
  frames that do not match the locked node/`ARG0` pair, and restart autodetection
  if the count crosses the threshold.
- Invalidate the restored candidate if responses are missed, if a different node
  reaches the confirmation threshold, if mismatch threshold is reached, or if
  sequence/response patterns suggest traffic belongs to another session.

This should have minimal ESPHome loop impact because reaffirmation can be a cheap
counter check on frames already being parsed. The main design question is safety:
the persisted value must never bypass fresh confirmation before control writes.

## Test and release hygiene

### Add protocol regression tests

- Add Python tests around `parse_capture()`, CRC matching, request/response pair
  summaries, and packet3 `TX req:` parsing.
- Add CI to run the checksum solver against bundled captures.
- Use the tests to protect future protocol refactors and capture-file moves.

### Add post-autodetection capture

- Add a new raw capture proving node address updates correctly with the
  autodetection PR applied.
- Include both diagnostic logs and raw UART where possible so TX echo and real
  controller responses can be distinguished.