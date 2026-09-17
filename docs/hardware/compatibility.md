# Known compatible boards

Identify your wall controller using the markings on both sides of its PCB.
**Front** means the LED/button side; **back** means the RJ12 connector side.

PCB version markings can differ between the front and back of the same board.
Record both when identifying your hardware. Protocol compatibility below reflects
tested boards; printed version numbers and dates alone do not establish compatibility.
Each row records a contributor's tested board, not necessarily a distinct revision.

## Protocol V2 — 9600 baud

**Confirmed fully working; the component's default protocol.**
Follow the [V2 setup guide](../guides/v2.1.md).

| Front (LEDs) | Back (RJ12) | PCB markings | Printed date / installation | Confirmed by |
|---|---|---|---|---|
| <img src="../assets/ashish-front.jpeg" width="180" alt="Ashish's LED-side PCB marked Zone Switch Touchpad V2.40 and 20190813"> | <img src="../assets/v2.1.png" width="180" alt="Rear of Ashish's PCB marked SMD Version V2.1 beside the RJ12 connector"> | Front: **Zone Switch Touchpad V2.40**<br>Back: **SMD Version V2.1** | `20190813` (13 August 2019); meaning unconfirmed. Installation unknown. | [@ashish-khokhar](https://github.com/ashish-khokhar) |
| <img src="../assets/threeseed-front.jpg" width="180" alt="Threeseed's LED-side PCB marked Zone Switch Touchpad V2.40 and 20190813"> | Not yet available | Front: **Zone Switch Touchpad V2.40**<br>Back: **Unconfirmed** | `20190813` (13 August 2019); meaning unconfirmed. Installation unknown. | [@threeseed](https://github.com/threeseed) · [Photo](https://github.com/jourdant/esphome-zoneswitch/issues/8#issuecomment-5706901948) · [Working configuration](https://github.com/jourdant/esphome-zoneswitch/issues/8#issuecomment-5018118560) |

Ashish's board carries both **V2.40 on the front** and **V2.1 on the back**.
These markings therefore do not necessarily identify different hardware models.
Threeseed's front photo has the same version and date markings, but its rear
marking has not yet been photographed.

The repeated `20190813` beside the printed version may be a design/revision date.
That is an inference from the photographs, not a confirmed manufacturing date or
an explanation of what each version number represents.

## Protocol V1 — 250000 baud

**Experimental support:** software control works, but external commands can leave
wall-controller LEDs out of sync and temporarily make physical buttons unresponsive.
Follow the [V1.0-T setup guide](../guides/v1.0-t.md).

| Front (LEDs) | Back (RJ12) | PCB markings | Printed date / installation | Confirmed by |
|---|---|---|---|---|
| <img src="../assets/v1.0-t-front.png" width="180" alt="LED and button side of Jourdant's V1.0-T wall controller"> | <img src="../assets/v1.0-t.png" width="180" alt="Rear PCB marked Zone Switch V1.0-T and 13/12 beside the RJ12 connector"> | Front: **No version marking visible**<br>Back: **Zone Switch V1.0-T** | `13/12` (date format unconfirmed); installed in **2014**, as reported by the owner. | [@jourdant](https://github.com/jourdant) |

## Contribute a compatibility report

Include clear photos of both PCB sides, all printed version/date markings, the
approximate installation year if known, and the protocol/UART settings that worked.
Describe whether software control, physical buttons and wall LEDs all work normally.
An unknown marking or similar connector alone is not a confirmed match.
See the [hardware notes](README.md) for wiring checks and
[contributing guide](../../CONTRIBUTING.md) for reporting details.
