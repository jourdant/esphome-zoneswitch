# Hardware identification and wiring

Start with the [known compatible boards](compatibility.md), including photographs
of both sides and contributor confirmations, then follow the
[V1.0-T](../guides/v1.0-t.md) or [V2](../guides/v2.1.md) guide.
The [V2 installation gallery](v2.md) preserves the original adapter wiring photos.

The photos establish board identity. They do not establish a common RJ12 pinout
for every revision. Verify connector orientation, A/B, ground and power before
connection. A/B labeling can vary between transceiver vendors; preserve the
mapping verified for your hardware.

The 24 VAC damper supply is not evidence that the wall-controller connector
uses 24 VAC. The captures and PCB identification photos do not by themselves
specify the wall supply. Use a measured/verified supply and an adapter rated for it.
Do not connect RS485 A/B directly to ESP32 GPIOs; use the RS485 transceiver.

For continuity/resistance tests, disconnect power. When testing a separately
USB-powered carrier, isolate the bus power feed as required by the carrier's
power design while retaining the required signal reference. Do not assume its
USB and bus power inputs can be paralleled.
