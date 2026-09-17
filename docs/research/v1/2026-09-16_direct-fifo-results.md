> Historical bench record. The user subsequently requested an experimental V1
> component despite the documented panel coexistence issue; the closing stop
> recommendation below records the earlier debugging decision.

# Direct-FIFO turnaround test

Replaced driver write + output abstraction with direct FIFO write and GPIO4
register control inside a bounded critical section. One/two-byte TX only, idle
check, 10 us DE setup, 200 us TX deadline, DE LOW before leaving the section.
RX driver and strict ACK 30 validation retained. No automatic retransmission.

Source evidence: [UART LL](https://github.com/espressif/esp-idf/blob/v5.5.5/components/hal/esp32s3/include/hal/uart_ll.h) and
[GPIO LL](https://github.com/espressif/esp-idf/blob/v5.5.5/components/hal/esp32s3/include/hal/gpio_ll.h). This is an exclusive-TX-owner
diagnostic, not a generic driver integration or a component implementation.

ESPHome 2026.8.2 / ESP-IDF 5.5.5 minimal local build passed (config 0x79baf18b).
Live config edited via Edge, diff reviewed, compiled and uploaded OTA. Device
reconnected with build timestamp 2026-09-16 19:59:36 +1000. UI test results:

| Local time | Test | ACK | TX windows (us) | ACK wait (us) | Reply |
|---|---|---|---|---|---|
| 20:00:49.485 | Q0: 00 | 30 | 55/53 | 164 | 08 00 81 02 03 84 05 17 |
| 20:01:00.237 | Q0: 00 | 30 | 55/53 | 149 | 08 00 81 02 03 84 05 17 |
| 20:01:12.831 | Q0: 00 | 30 | 54/53 | 153 | 08 00 81 02 03 84 05 17 |
| 20:01:26.979 | H2: 02 | 30 | 55/52 | 157 | 08 00 01 02 03 84 05 97 |
| 20:01:37.753 | Q0: 00 | 30 | 55/53 | 149 | 08 00 01 02 03 84 05 97 |

Five consecutive controlled exchanges completed with ACK 30 and full state
replies, no 98 observed. Three initial queries retained zones 2/5 ON. One H2
toggled reported Zone 2 OFF; the final Q0 retained that state, Zone 5 ON.
Measured first TX windows fell from ~128–130 us to 54–55 us. These are software
durations; they neither prove an electrical edge margin nor long-term reliability.

Testing paused with reported Zone 2 OFF for user observation. User confirms
Zones 2 and 5 remained lit: the successful external toggle plus query did not
refresh the idle wall display. No restart or physical keypress occurred during
this check. Other zones were not toggled.

Next requested observation: one physical Zone 3 press, to determine whether a
panel-initiated transaction refreshes the entire displayed state, including stale
Zone 2. This is a hypothesis test; reply processing only during the panel's own
transactions has not yet been proven.

User reports the first physical Zone 3 press did nothing; LEDs stayed 2/5 ON.
Corresponding capture:

```text
[20:03:02.908][D][uart_debug:113]: [UART] <<< C0 30
```

The physical transaction is incomplete (no 04 or state reply visible). Therefore
the unresolved behavior includes physical transaction disruption, not just stale
display. No software transmissions have occurred after 20:01:37.753. A second
physical Zone 3 press was requested while passively listening to test recovery.

Second press also did nothing. User says several presses will eventually work.
Observed second capture:

```text
[20:04:45.279][D][uart_debug:113]: [UART] <<< C0 30
```

No complete Zone 3 transaction is captured yet. Clean carrier transactions do
not establish that the wall panel receives the ACK correctly, or rule out
electrical loading/interference. Requested an isolation check: remove carrier
A/B only, without resetting the wall panel or its original HVAC connection,
then try one physical Zone 3 press. Carrier remains receive-only; no further
software probes. Awaiting result before choosing electrical or protocol follow-up.

Isolation result: with carrier A/B disconnected, one Zone 3 press still did
nothing. Two further rapid presses caused the wall display to refresh and show
Zone 2 OFF. No panel restart was reported. The exact final Zone 3 state is not
specified, and no bus capture is available while A/B is disconnected.

Continued symptoms after physical isolation argue against ongoing carrier bus
loading/drive as the sole immediate cause. They support a lingering protocol or
parser state, but do not rule out an earlier electrically disturbed exchange.
The short clean-carrier test run does not establish compatibility with the panel.

Next isolation test: reconnect A/B and establish one-press physical operation
before software TX. Then test one acknowledged 00 query alone, followed by a
physical press, to distinguish query-induced disruption from the 02 toggle.
Do not combine query and toggle in this next test. Awaiting restored baseline.

User subsequently confirmed Zone 3 works after the reconnect/baseline request.
Sent exactly one Q0 through HA, with no software toggle:

```text
[20:15:13.281][I][v1_test:165]: H6: TX C0 -> RX ACK 30 -> TX 00; DE LOW; windows 55/53 us; ACK wait 159 us
```

Requested exactly one subsequent physical press of the same button and whether
its LED changes on that first press. The user answered no. Later inspection
confirmed the query's full reply and the subsequent incomplete physical exchange:

```text
[20:15:13.577][D][uart_debug:113]: [UART] <<< 08 00 81 02 03 84 05 17
[20:18:40.081][D][uart_debug:113]: [UART] <<< C0 30
```

Thus one external acknowledged 00 query reproduces subsequent physical-button
failure from a user-confirmed working baseline, without any software toggle.
This is not a toggle-mask-only issue. No further software commands queued or sent.
ACK and valid state reply are insufficient acceptance criteria for coexistence.
Do not enable periodic polling or implement production V1 TX on this evidence.
Remaining candidates include transaction timing/ownership and panel parser state;
the shared two-wire capture cannot prove which peer drives each observed byte.
