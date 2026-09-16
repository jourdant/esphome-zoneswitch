# Remaining work

## V1 wall-panel coexistence

- Attribute wall TX/RX/DE with the Saleae Logic 8; shared-bus UART captures cannot identify every sender.
- Resolve stale LEDs and temporarily unresponsive physical buttons after external queries/control.
- Establish reliable collision/response attribution before introducing retry behavior or frequent polling.
- Test checksum hypotheses with evidence beyond changes in bit 7 of indexed zone bytes.
- Extend the demonstrated integrated query/OFF/ON cycle into sustained bench validation.

## V2 extensions

V2 is confirmed fully working on V2.1 hardware. These are further coverage and
investigation opportunities, not blockers to the established protocol support:

- Capture any response `ARG0` variant beyond the observed `0x01` before claiming compatibility with it.
- Add documented bench evidence for persisted-candidate invalidation and sequence-mismatch behavior on a shared bus.
- Add capture fixtures from the second/third source logs, particularly differing node addresses and TX debug records.

## Project follow-up

- Validate this refactor on both physical revisions before promoting a new release.
- Owner decision on project licensing.
- Consider additional diagnostics only when field evidence justifies them (node-lock state, resynchronization count, deferred TX).

Current diagnostics, replay tests, contribution guidance and release procedure
are implemented. CI/automated check orchestration was intentionally excluded
from this project-organization change.
