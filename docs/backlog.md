# ZoneSwitch Backlog

Open engineering and reverse-engineering tasks that are worth keeping visible,
but are not required for the current safe-control implementation.

## Protocol validation

### Validate response `ARG0` variants with new captures

Current captures confirm status responses with `ARG0=0x01`:

`AA NODE 00 SEQ 81 01 MASK CHK 55`

The component can learn the response-side `ARG0` value dynamically, and the
checksum solver reports observed response `ARG0` values. The remaining work is
to validate whether any value other than `0x01` appears in real traffic.

Next steps:

- Capture startup/idle/toggle traffic after the node autodetection PR is applied.
- If another `ARG0` value appears, add the backing capture before documenting it
  as a confirmed protocol variant.
- Keep implementation comments aligned with capture-backed evidence.

## Node autodetection and startup latency

### Validate persisted node fallback and invalidation

The component can persist the last confirmed node and restore it on boot as an
untrusted fallback candidate. Fresh confirmations are still required before zone
writes are enabled. The remaining work is to validate the behavior on hardware
and decide whether any additional invalidation policy is needed.

Possible strategy:

- Confirm restored candidates speed up active polling in real deployments.
- Confirm stale restored candidates are cleared by `node_mismatch_threshold` or
  normal missed-response handling.
- Confirm writes remain blocked until fresh status confirmations lock the node.
- Build on the runtime `node_mismatch_threshold` guard: count valid-looking status
  frames that do not match the locked node/`ARG0` pair, and restart autodetection
  if the count crosses the threshold.
- Consider whether time-based invalidation is useful in addition to mismatch and
  missed-response handling.

This should have minimal ESPHome loop impact because reaffirmation can be a cheap
counter check on frames already being parsed. The main design question is safety:
the persisted value must never bypass fresh confirmation before control writes.

## Test and release hygiene

### Convert captures to a standard fixture format

The raw captures currently mix ESPHome UART debug lines, narrative comments, and
component debug output. That is useful for humans, but awkward for regression
tests.

Create a converter that reads the existing capture markdown files and emits a
normalized machine-readable fixture format, probably JSON Lines or JSON:

- one record per frame or event
- source filename and line number
- timestamp where available
- direction/source type, e.g. `raw_uart`, `component_tx_debug`, `diagnostic`
- normalized 9-byte frame bytes where available
- parsed fields: `dst`, `src`, `seq`, `cmd`, `arg0`, `arg1_or_mask`, `checksum`
- optional annotation fields copied from nearby headings/comments, such as
  `all_off`, `pressed_z1_on`, or `pre_autodetection_pr`

Regression tests should run against this normalized format rather than scraping
the original markdown each time. Keep the raw markdown as the audit trail and the
standard fixtures as derived test data.

### Add protocol regression tests

- Add Python tests around `parse_capture()`, CRC matching, request/response pair
  summaries, and packet3 `TX req:` parsing.
- Add CI to run the checksum solver against bundled captures.
- Use the tests to protect future protocol refactors and capture-file moves.

### Add passive-mode offline timeout

Offline detection currently depends mainly on active polling response misses. In
passive-only mode, a gateway can receive one valid status frame, become online,
and then remain online even if the bus goes quiet.

Add a `status_timeout` or similar option that tracks `last_status_ms_` and marks
the component offline when no valid status frame has been seen for a configured
period. This should be independent of active polling.

### Enhance and validate active response sequence policy

Sequence mismatches are currently logged, but valid status frames are still
accepted for state refresh. Keep this behavior for now, but validate it on a bus
with a physical touchpad and ESPHome both present.

Possible enhancement:

- passive mode: accept any checksum-valid status frame for state refresh
- active poll waiting: use mismatched sequence as a warning, but allow state refresh
- active write waiting: require the expected sequence before clearing write wait
  state, so unrelated traffic cannot confirm a write

Capture evidence is needed before tightening this further.

### Expose more diagnostics

Add optional diagnostics to improve field debugging:

- locked node address vs candidate/restored node address
- candidate confirmation count
- node mismatch count
- last status age
- TX deferred count
- resync count
- last response sequence and last TX sequence

These will make it easier to diagnose wiring, TX echo, stale restored nodes, and
coexistence issues without enabling verbose logs.

### Add post-autodetection capture

- Add a new raw capture proving node address updates correctly with the
  autodetection PR applied.
- Include both diagnostic logs and raw UART where possible so TX echo and real
  controller responses can be distinguished.