# Broadcast and multi-drive start review, 12 November 2026

The owner asked two questions. Do the ESS drives accept non-addressed
(broadcast) commands, so the library can start several drives together? Can the
library program the drive inputs for a shared hardware start? This is a source
and code review. No frame was sent and no firmware was changed.

## Result

- **Broadcast is not documented.** No ESS document describes a broadcast, group,
  multicast, synchronous or "start all" command, or any special address.
  Address 0 is only "Reserved for the system". No document says the drive
  accepts, ignores or answers a frame sent to address 0. It is not contradicted
  either. What firmware `0x0029` does with such a frame is unknown until tested.
- **The documented way to start several drives together is a shared input
  edge.** The manual's own multi-drive wording ("control up to 15 ESS23-RS
  series drives at the same time") means 15 addressed drives on one bus. It does
  not describe simultaneous motion.
- **The library can address several drives, one transaction at a time.** The
  core and owner support this. The standalone firmware and console work on one
  selected drive at a time, and the console admits only passive input functions.
  The [multi-drive promptset](../prompts/ess_multi_drive/README.md) holds the
  remaining work.

## Sources checked

Page numbers are physical PDF pages. The extraction scrambles several pages
(function manual p6, p13-14, p16, p22, p25, p69-71). Those pages were checked as
rendered images.

| Source | How checked | Finding |
| --- | --- | --- |
| [Function manual V1.0](../vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf), first edition 2025-03-05 | Rendered p2, 4-8, 10-14, 16-18, 20-25, 27, 29, 68-71, 74, 75, 77; contact sheets of p32-67 (homing) and p80-90 (DM-PR); text layer of all 90 pages | No broadcast or sync. FC08 diagnostics are unsupported (exception 02, p11), so no broadcast counter exists. Quotes below |
| [Hardware manual](../vendor/ESS23-RS1020_Series_Bus_Integrated_Motor_Hardware_Manual.pdf) | All 12 pages rendered | Address 0 "Reserved for the system" (p11). Input electrical limits (p5, p8, p9) |
| [RS20](../vendor/ESS23-RS20_Full_Datasheet.pdf) and [RS10](../vendor/ESS23-RS10_Full_Datasheet.pdf) datasheets | Rendered | Mechanical and electrical tables only; no communication content |
| [Debug software V1.2.7](../vendor/software/README.md) | Extracted and read as bytes, never executed | The installer payload could not be decoded; the GUI's own strings were not recovered. No evidence either way |
| Probable OEM manuals (Shenzhen YAKO: YSS-R V1.0, SSD2505PR V1.1, YKD2608PR V1.3, YKD2405PR V1.3) | Web, not preserved here | Same register layout and model word `0x0305`; same "address 0 is reserved for the system" sentence; no broadcast. The OEM link is an inference from matching registers |
| StepperOnline DM882RS manual, ConSys device page for DM556RS/DM882RS/CL57RS/CL86RS | Web, not preserved here | Address 0 is broadcast and a synchronous mode exists, but this is a different family with a `0x6000`-style register map. It does not apply to ESS |
| StepperOnline web site | Every page and PDF returned HTTP 403 | The current ESS download list is not confirmed. No function manual newer than V1.0 was found |

Quoted facts from the ESS documents:

- Hardware p11: "The user can use MODBUS bus to control up to 15 ESS23-RS series
  drives at the same time. The drive communication address setting adopts 4-bit
  DIP switch, the address setting range is 0~15, where address 0 is Reservedd for
  the system". The DIP table gives custom address for all OFF, 1-14, and factory
  reset for all ON.
- Function p4: "Each slave device has a unique address... only the node
  designated as the master node can initiate a command." p5: "Each slave node in
  the Modbus network needs to be set to a different address".
- Function p6 and p69, register `0x0013`: "0 ~ 255 : custom slave address", RW,
  default 0, effective only with all address DIP switches OFF. The meaning of
  0 and 248-255 is not given.
- Function p7-8: only FC03, FC06 and FC10 are documented. FC06: "After receiving
  the command, the slave returns the same command for confirmation." p11 lists
  exception 02 for codes other than 03, 06 or 10; the p12 table says "other than
  03 and 06".
- Every example frame uses address 01; none uses 00.
- `0x0027` (p13, p70-71) has bits 0-4, 8 and 9 only. No sync bit exists.
- Function p13: positioning, speed and homing can be controlled through
  `0x0027` or through external inputs. "The multi-stage position control and
  multi-stage speed control functions can only be controlled by external input
  signals."

The [Modbus serial line standard](../standards/Modbus_Serial_Line_V1.02.pdf)
(p7-8, p10) defines address 0 as broadcast. A broadcast is a write, gets no
reply, and must be followed by a turnaround delay, typically 100-200 ms. The
ESS manual neither claims nor denies that conformance.

## Why broadcast needs a test before use

- An unanswered write is never confirmed. Every drive must be read back by unicast.
- Every drive on the bus acts on it, including drives the application did not
  mean to address. A broadcast start would start any drive holding a staged
  profile.
- A drive that answers address 0 would collide with other drives answering.
  That makes broadcast unusable on a shared bus.
- The ESS documents give no turnaround delay after a broadcast.
- The bench drive answers at node 1 with all address DIP switches OFF and
  custom node `0x0013` = 0
  ([ess_release_05](../archive/reports/ess_release_05_2026-10-04.md)). A drive
  whose stored custom node is 0 might treat address 0 as its own. A result
  therefore applies only to the recorded address configuration.

[Multi-drive 01](../prompts/ess_multi_drive/01_broadcast_acceptance.md) defines
a bounded single-drive test of these points. Use broadcast only where
simultaneity matters, that is start and stop. Settings stay unicast, where
each write is confirmed.

## Three ways to start several drives

1. **Sequential unicast starts.** These work now in the core. Stage each drive
   and confirm it, then send the starts back to back. Each FC06 start and its
   echo take 16 bytes, about 1.4 ms of wire time at 115200 8N1, plus the drive's
   reply delay and inter-frame gaps. Those extra delays have not been measured.
   The standalone application cannot yet do this for a group.
2. **Broadcast start after unicast staging.** This is possible only if firmware
   `0x0029` accepts address 0 and stays silent.
3. **Shared input edge.** Function manual p27 lists:
   - input function 7, "Position mode movement": the drive runs the staged
     movement parameters. p17 describes this from standstill and calls it
     level-active ("When a valid level signal is given to this terminal");
     p27 calls it edge-active. `0x0050` makes input-started moves relative or
     absolute (p75). An absolute target is safer, because a repeated trigger
     then repeats the same target;
   - function 12, PT trigger (edge), and function 13, PV trigger (level), with
     PIN0-PIN3 (functions 14-17) selecting a stored segment;
   - p29, method 1: function 7 switches a speed move into a fixed-length move
     using `0x0024/0x0025`.

   Function 7 from idle uses the ordinary staged profile, so it needs no stored
   target writes. With four inputs, PIN selection reaches at most 8 segments
   (p23-24). PIN levels must be stable for 5 ms before and after the PT edge
   (p23). The inputs:
   - are opto-isolated, with a standard signal of 24 V and at least 10 mA of
     drive current (hardware p5, p9);
   - need a level pulse longer than 10 ms (p9);
   - at each power-on: "X0-X3 defaults to the unspecified state. At this time,
     the input signal is invalid" (p9). The register table lists defaults 1, 2,
     3, 0 for `0x0041-0x0044` (function p73-74), and the bench read those
     values. The two statements conflict.

   No document gives the delay from input edge to motion.

## Library input and segment support

| Capability | Core API | Standalone console | Hardware evidence |
| --- | --- | --- | --- |
| Read input/output state | Yes | `read state`, `monitor` | Read only |
| Input function 0-17 | All admitted (`DriverSettings.cpp:96`) | Functions 0-3 only, on terminals declared unconnected (`ProbeApp.cpp:1294-1302`) | Stored readback on unconnected terminals |
| Input polarity | Yes | Only with function 0 before and after, unconnected | Stored readback |
| Input filter `0x0108` | Yes (tuning) | Values 2 and 3 | Stored readback; timing meaning undocumented |
| Output function | 0-5, 9, 10; 6-8 rejected; 11 unresolved | Unconnected outputs only | Stored readback |
| Custom output `0x004F` | Canonical mapping only, motor stopped | Unconnected outputs only | Unloaded output only |
| Stored PT target pairs | Guarded: no admissible write form | Refused | Never written |
| PT/PV speed and ramps | Yes; negative PV speed guarded | `segment ... set` | Record 1 and 16 scalars |
| Position mode `0x0050`, PV trigger mode `0x0051` | Yes | Always refused | None |
| Serial start of a stored segment | Not documented by the vendor | None | None |
| Observe input-started motion | Raw state polling only | `read state`, `monitor` | None |

Assigning any input function above 3 also blocks ordinary console moves
(`MotionReadinessApp.h:73-74`), segment writes and some setters. The
[I/O contract](../ess_io.md) and [segment guide](../ess_segments.md) give the
detailed reasons.

## Questions for the vendor

The owner can send these:

1. Does ESS firmware `0x0029` act on Modbus broadcast writes (slave address 0)
   for FC06 and FC10? Does it stay silent? Which registers or commands apply?
2. What do "address 0 is reserved for the system" and custom address values 0
   and 248-255 in `0x0013` mean?
3. Is input function 7 edge-active (p27) or level-active (p17)? Does a held
   level repeat the move? What is the delay from input edge to motion start?
4. Do input/output function assignments survive power-up after a save? The
   hardware manual says "X0-X3 defaults to the unspecified state" at each
   power-on, but the register defaults are 1, 2, 3, 0.
5. Is there any synchronized multi-axis start for the ESS-RS family?

## Follow-up

The [backlog](../backlog.md#multi-drive-broadcast-and-input-triggered-motion)
and the [multi-drive promptset](../prompts/ess_multi_drive/README.md) carry the
work. The bench has one drive with power and RS485 only. Every multi-drive and
input-trigger claim needs a second drive or a wired trigger.
