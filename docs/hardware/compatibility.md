# Board identification and compatibility

Identify your wall controller using the markings on both sides of its PCB.
**Front** means the LED/button side; **back** means the RJ12 connector side.

PCB version markings can differ between the front and back of the same board.
Record both when identifying your hardware. Protocol compatibility below reflects
tested boards; printed version numbers and dates alone do not establish compatibility.
Each row records a contributor's reported board, not necessarily a distinct revision.
Boards are grouped by confirmed or expected protocol, with untested entries explicitly
marked. **Date added** is the date the entry was first added to this document
(YYYY-MM-DD), not the PCB date or installation date.

Within each protocol table, rows are sorted by PCB version in ascending order,
using the front-side marking where available and otherwise the back-side marking.

## Protocol V2 — 9600 baud

**The component's default protocol; confirmed fully working on the tested boards below.**
The V2.3 entry is grouped here provisionally and is awaiting testing.
Follow the [V2 setup guide](../guides/v2.1.md).

| Date added | Front (LEDs) | Back (RJ12) | PCB markings | Printed date / installation | Contributor / status |
|---|---|---|---|---|---|
| 2026-09-17 | <img src="../assets/mattaustin-front.jpg" width="180" alt="Mattaustin's LED-side PCB marked Zone Switch Touch Pad V2.3 and 2015.06"> | <img src="../assets/mattaustin-back.jpg" width="180" alt="Rear of Mattaustin's PCB with the modular connector and no visible version marking; contributor-redacted labels preserved"> | Front: **Zone Switch Touch Pad V2.3**<br>Back: **No version marking visible** | `2015.06` (June 2015); meaning unconfirmed. Installation unknown. | [@mattaustin](https://github.com/mattaustin) · [Photos and report](https://github.com/jourdant/esphome-zoneswitch/issues/8#issuecomment-5710688310) <br>**Awaiting V2 confirmation** |
| 2026-09-17 | <img src="../assets/ashish-front.jpeg" width="180" alt="Ashish's LED-side PCB marked Zone Switch Touchpad V2.40 and 20190813"> | <img src="../assets/v2.1.png" width="180" alt="Rear of Ashish's PCB marked SMD Version V2.1 beside the RJ12 connector"> | Front: **Zone Switch Touchpad V2.40**<br>Back: **SMD Version V2.1** | `20190813` (13 August 2019); meaning unconfirmed. Installation unknown. | [@ashish-khokhar](https://github.com/ashish-khokhar) <br>Confirmed working |
| 2026-09-17 | <img src="../assets/threeseed-front.jpg" width="180" alt="Threeseed's LED-side PCB marked Zone Switch Touchpad V2.40 and 20190813"> | Not yet available | Front: **Zone Switch Touchpad V2.40**<br>Back: **Unconfirmed** | `20190813` (13 August 2019); meaning unconfirmed. Installation unknown. | [@threeseed](https://github.com/threeseed) · [Photo](https://github.com/jourdant/esphome-zoneswitch/issues/8#issuecomment-5706901948) · [Working configuration](https://github.com/jourdant/esphome-zoneswitch/issues/8#issuecomment-5018118560) <br>Confirmed working |

Ashish's board carries both **V2.40 on the front** and **V2.1 on the back**.
These markings therefore do not necessarily identify different hardware models.
Threeseed's front photo has the same version and date markings, but its rear
marking has not yet been photographed.

The repeated `20190813` beside the printed version may be a design/revision date.
That is an inference from the photographs, not a confirmed manufacturing date or
an explanation of what each version number represents.

At the time of the report, @mattaustin had not connected an ESPHome adapter and was
gathering components. The [maintainer's reply](https://github.com/jourdant/esphome-zoneswitch/issues/8#issuecomment-5710800326)
suggests V2 as the expected protocol, but neither the protocol nor its baud rate
has been confirmed on this board. Its printed date is not a verified manufacturing
or installation date. The front and back photos are the contributor's original
second and first attachments respectively; existing label redactions are preserved.

## Protocol V1 — 250000 baud

**Experimental support:** software control works, but external commands can leave
wall-controller LEDs out of sync and temporarily make physical buttons unresponsive.
Follow the [V1.0-T setup guide](../guides/v1.0-t.md).

| Date added | Front (LEDs) | Back (RJ12) | PCB markings | Printed date / installation | Contributor / status |
|---|---|---|---|---|---|
| 2026-09-17 | <img src="../assets/v1.0-t-front.png" width="180" alt="LED and button side of Jourdant's V1.0-T wall controller"> | <img src="../assets/v1.0-t.png" width="180" alt="Rear PCB marked Zone Switch V1.0-T and 13/12 beside the RJ12 connector"> | Front: **No version marking visible**<br>Back: **Zone Switch V1.0-T** | `13/12` (date format unconfirmed); installed in **2014**, as reported by the owner. | [@jourdant](https://github.com/jourdant) <br>Software control confirmed; experimental |

## Contribute a compatibility report

Include clear photos of both PCB sides, all printed version/date markings, the
approximate installation year if known, and the protocol/UART settings that worked.
Describe whether software control, physical buttons and wall LEDs all work normally.
An unknown marking or similar connector alone is not a confirmed match.
See the [hardware notes](README.md) for wiring checks and
[contributing guide](../../CONTRIBUTING.md) for reporting details.
