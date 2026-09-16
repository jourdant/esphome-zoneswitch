# ZoneSwitch V1.0-T protocol (experimental)

## Evidence

Bench captures on 2026-09-16 established 250000 baud, 8N1, noninverted UART.
The PCB marking is V1.0-T; this is separate from the V2.1 protocol.
[All zones](../../research/v1/2026-09-16_user-all-six-zones.md),
[startup](../../research/v1/2026-09-16_startup-captures.md), and
[acknowledged software transactions](../../research/v1/2026-09-16_direct-fifo-results.md)
are the source observations. Hardware controls were installed on zones 2–5.

Shared-bus physical capture: `C0 30 MASK 08 Z1 Z2 Z3 Z4 Z5 Z6 CHECK`.
This combined trace does not identify the sender of every byte. Software control
worked with TX C0, RX 30, TX MASK; sending C0/30/MASK in one write did not toggle.
MASK is 00 to query, or one of 01/02/04/08/10/20 to toggle zones 1–6.
Startup captures include C0 30 00. No node address or sequence field is observed.

Status is eight bytes: `08 Z1 Z2 Z3 Z4 Z5 Z6 CHECK`. Each zone byte's low seven
bits equal its index 0–5; bit 7 means ON. Example:

- `08 00 81 02 03 84 05 17`: zones 2 and 5 ON.
- `08 00 01 02 03 84 05 97`: only zone 5 ON.

The modulo-256 sum of the first seven bytes matches CHECK in all captures.
Only bit 7 varies in these observations; this cannot uniquely identify the
checksum against every alternative. The driver validates that candidate and
all six structural indices before publishing state. Prefixes are tolerated,
including an 08 zone-4 mask immediately preceding the 08 status header.

## Driver behavior

V1 currently supports only ESP32-S3 with ESP-IDF. A bounded critical section
writes one byte directly to the UART FIFO, waits at most 200 us for TX idle,
and immediately releases DE. The ACK wait is at most 10 ms with interrupts
enabled. No logs execute between ACK reception and mask transmission. The
installed ESPHome UART driver continues handling RX. The UART must have a
one-byte RX threshold and no other TX writers. This avoids the late receive
turnaround seen with the earlier driver/flush experiments.

The shared component is the only direction-pin owner. It starts in receive.
After a 20 ms receive quiet guard, each transaction sends C0, requires ACK 30,
and sends one mask. A full status must arrive within 250 ms. Incomplete received
frames expire after 20 ms; transactions are separated by at least one second.
Startup requests one status. Periodic polling defaults off. The refresh button
requests one status; repeated refresh presses are coalesced.

An absolute ON/OFF request first queries state. If already satisfied it does not
toggle. Otherwise it sends one toggle and waits for reported state. Timeout,
unexpected ACK or TX failure cancels intent without retry. Later unsolicited
status cannot revive a cancelled write. Requests while busy are rejected.
States are never changed optimistically. The spill guard can suppress closing
the configured final spill zone. Physical airflow is not measured.

## Unresolved coexistence

External toggles changed controller-reported state but wall LEDs remained stale.
A startup-style query did not refresh the running panel. Physical presses after
an external query sometimes emitted only C0 30, with no mask or status. Repeated
physical presses or restarting the panel recovered it. Query-only traffic also
reproduced the failure; it is not merely an LED-display issue. The user accepted
this limitation for initial software control. No fix is claimed here.

There is no sequence correlation in observed V1 status. The quiet guard reduces
collision opportunities but cannot establish ownership of a response. A logic
analyser capture of wall TX/RX/DE remains follow-up work. V1 is opt-in and does
not change V2's default framing or address learning.

Technical references: [ESPHome UART](https://esphome.io/components/uart/),
[ESP-IDF UART LL](https://github.com/espressif/esp-idf/blob/v5.5.5/components/hal/esp32s3/include/hal/uart_ll.h).
