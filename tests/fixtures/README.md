# Capture fixtures

[captures.tsv](captures.tsv) is a UTF-8 tab-separated replay file. Each record has:

| Column | Meaning |
|---|---|
| `id` | Unique readable test-case name |
| `protocol` | `v1` or `v2` |
| `bytes` | Original captured bytes, hexadecimal and space separated |
| `expected_mask` | Expected accepted zone-state mask in hexadecimal |
| `source` | Repository-relative raw capture path and one-based line |
| `timestamp` | Original local capture timestamp; no inferred timezone |

V1 rows preserve each physical button exchange from the all-six-button capture.
V2 rows preserve request/status pairs from the first idle capture. The host test
feeds these bytes into the actual production parsers (V1 byte by byte), comparing
the expected state and counters. The source-metadata test verifies every record
still matches its cited raw source line. Captures are shared-bus observations;
RX direction at the observer does not identify each sender.

Keep original logs in `docs/research`. Add new rows with unique IDs, verbatim bytes,
source line and expected state established from the observation/spec. Synthetic
malformed input and transaction simulations live in `protocol_test.cpp`; do not
label generated packets as bench captures. No converter or test runner is needed
to replay this small, reviewable format.
