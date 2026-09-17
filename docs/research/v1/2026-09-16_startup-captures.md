# Two wall-panel restart captures

User reports toggling Zone 2 twice, then unplugging/replugging wall-panel power,
repeated twice. Capture followed discussion of independently powering the carrier
over USB with its field VIN wire removed. Actual rewiring was not explicitly
confirmed. Safe-mode message at 19:48:08 is consistent with a carrier boot roughly
one minute earlier; it does not by itself show a reboot at either panel restart.

```text
[19:47:37.992][D][uart_debug:113]: [UART] <<< C0 30 02 08 00 01 02 03 84 05 97
[19:47:40.037][D][uart_debug:113]: [UART] <<< C0 30 02 08 00 81 02 03 84 05 17
[19:47:49.985][D][uart_debug:113]: [UART] <<< C0 30 00 08 00 81 02 03 84 05 17
[19:48:08.498][I][safe_mode:154]: Boot seems successful; resetting boot loop counter
[19:48:11.273][D][uart_debug:113]: [UART] <<< C0 30 02 08 00 01 02 03 84 05 97
[19:48:11.767][D][preferences:254]: Writing 1 items: 0 cached, 1 written, 0 failed
[19:48:13.844][D][uart_debug:113]: [UART] <<< C0 30 02 08 00 81 02 03 84 05 17
[19:48:23.052][D][uart_debug:113]: [UART] <<< C0 30 00 08 00 81 02 03 84 05 17
```

Both 02 pairs show Zone 2 OFF then ON. Both restart exchanges instead contain 00
and return the same state (zones 2 and 5 ON). This supports 00 as a no-toggle
state query following the C0/30 handshake. Shared-bus UART logging does not assign
each byte to an original sender. An externally initiated query has not yet been
shown to refresh the wall panel's LEDs; the panel may only process replies within
its own transactions. No further startup command is visible in this excerpt.

Next bounded test: acknowledged 00 query, then one acknowledged Zone 2 toggle and
one acknowledged 00 query, observing LED behavior. No automatic retry or polling.

## Live follow-up through ESPHome and Home Assistant

Added Q0 (internal probe kind 6, logged as H6) to the existing diagnostic bundle.
ESPHome Builder compiled and installed it over OTA; the reconnect logs confirm
firmware built 2026-09-16 19:50:36 +1000, 250000 8N1 and RX threshold 1.

```text
[19:51:36.841][W][v1_test:149]: H6: unexpected RX 98; NO zone byte sent; DE LOW
[19:51:52.504][I][v1_test:127]: T0: sent C0; DE LOW; TX window 130 us
[19:51:52.523][D][uart_debug:113]: [UART] <<< 30
[19:52:05.707][I][v1_test:158]: H6: TX C0 -> RX ACK 30 -> TX 00; DE LOW; windows 128/60 us; ACK wait 125 us
[19:52:05.727][D][uart_debug:113]: [UART] <<< 08 00 81 02 03 84 05 17
[19:52:17.086][W][v1_test:149]: H2: unexpected RX 98; NO zone byte sent; DE LOW
```

The successful Q0 returns unchanged state (zones 2 and 5 ON), corroborating the
no-toggle query interpretation. The planned Zone 2 change was aborted before
02 was sent, so the subsequent display-refresh test could not be performed.
Testing stopped. No zone masks were transmitted during this follow-up.

Intermittent 98 instead of 30 requires investigating RX enable/turnaround timing.
A one-bit-late sample of 30 with the stop bit shifted into the MSB would give 98;
this is a timing hypothesis, not a diagnosis from buffered UART logs. Retain
strict ACK validation. Do not accept 98 as equivalent to 30 or retry toggles
automatically. The successful exchange does not establish reliability.
