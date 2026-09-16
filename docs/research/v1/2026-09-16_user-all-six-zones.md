# All-six-button UART capture

User-provided 2026-09-16, 250000 baud, 8N1, receive-only. User identifies the first twelve presses as zone 1 ON/OFF through zone 6 ON/OFF in order. Final presses select zone 2 then zone 5.

```text
17:55:10.870 C0 30 01 08 80 01 02 03 04 05 97
17:55:16.424 C0 30 01 08 00 01 02 03 04 05 17
17:55:20.146 C0 30 02 08 00 81 02 03 04 05 97
17:55:24.419 C0 30 02 08 00 01 02 03 04 05 17
17:55:27.586 C0 30 04 08 00 01 82 03 04 05 97
17:55:31.055 C0 30 04 08 00 01 02 03 04 05 17
17:55:33.313 C0 30 08 08 00 01 02 83 04 05 97
17:55:35.885 C0 30 08 08 00 01 02 03 04 05 17
17:55:38.707 C0 30 10 08 00 01 02 03 84 05 97
17:55:41.419 C0 30 10 08 00 01 02 03 04 05 17
17:55:43.003 C0 30 20 08 00 01 02 03 04 85 97
17:55:44.947 C0 30 20 08 00 01 02 03 04 05 17
17:55:47.162 C0 30 02 08 00 81 02 03 04 05 97
17:55:48.876 C0 30 10 08 00 81 02 03 84 05 17
```

All 14 eight-byte replies pass the additive checksum and contain six indexed state bytes. User says only “zones 2-4” have been set up on the faceplate, and calls 1 and 6 “the other 2”; this leaves zone 5's installed role ambiguous. Do not infer damper population from these logs. Logical states are observed for all six positions.
