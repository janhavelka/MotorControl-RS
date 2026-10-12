# Console grammar and reporting boundaries, 12 October 2026

[Maintainability prompt 05](../prompts/maintainability/05_console_boundaries.md)
starts from `c656564`, after the completed host-routing audit. This inventory
was recorded against the actual source before moving the HOME grammar or
formatting. The extraction is a structural change to the shared Arduino/native
ESP-IDF application console; it adds no motor behavior or qualification.

## Responsibility and behavior inventory

| Boundary | Owner and observable contract |
| --- | --- |
| Input framing and tokenization | Console rejects invalid/overlong lines as a whole, bounds tokens, handles CR/LF/CRLF and selects correlated JSON before reporting input errors. |
| Command catalogue | The single `COMMANDS` table in `ProbeConsole.cpp` supplies names, syntax, effects and help/coverage metadata; internal typed-read aliases remain explicit. |
| Pure family grammar | HOME converts bounded argument tokens into the existing typed request and optional address. It performs no callback or motor operation. |
| Availability and admission | Console checks hooks, cached default address, axis query and correlation capacity, then calls `beginWork` and the existing `startHome` callback in the original order. |
| Host requests and results | `AxisConsole.h` defines host-only typed requests/views. Host callbacks consume requests synchronously; `ProbeApp::startHome` owns prerequisites, public sequence preparation, owner admission and retained operation storage. |
| Correlation and urgent stop | Console allocates command IDs, rejects duplicates and keeps nine ordinary correlations plus the existing reserved stop slot through the shared `availableSlot` selector. |
| Bounded evidence formatting | Family formatters borrow contexts only during their call and write into explicit caller-owned capacity. They do not retain evidence pointers. |
| Presentation and output | `ConsoleText.cpp` renders bounded JSON into human text with borrowed spans and bounded depth. Console owns human/JSON selection, complete-line pending output, sink acceptance and terminal untracking. |

HOME preserves this first-error and callback order: pending output drops the
command; exact `home methods` needs no motor hooks; missing `startHome`,
`snapshot` or `axis` yields unavailable before motion argument validation;
argument count and literal `zero` precede numeric fields; method syntax/exact
int16 conversion and search/return/ramp ranges precede optional address
validation. An omitted address comes from the cached snapshot, then its range
is checked. Axis QUERY follows address validation and precedes capacity
admission. `beginWork` runs only when an ordinary slot is available;
`startHome` runs once and only OK is tracked. Configuration generation still
comes from the queried axis. Invalid grammar calls no motor callback.

## Formatting outcomes recorded before extraction

| Existing path | Failure and delivery behavior |
| --- | --- |
| HOME methods listing | Any bounded append failure reports local `output_full`; no operation was admitted. |
| HOME terminal/result body or evidence | Any append failure returns false without emitting or clearing correlation. The host retains its complete evidence. |
| HOME host-serial append | Returns false with the same retained correlation; no partial line is emitted. |
| Action, driver, move and velocity terminals | Body/evidence or serial append failure likewise returns false and retains correlation. |
| Probe initial `snprintf`, general typed-read body, recovery initial `snprintf` | Overflow emits the existing terminal formatting-error reply. For live delivery, pending terminal bookkeeping untracks after sink acceptance; immediate acceptance clears the slot immediately. |
| Later probe/read/recovery append | Returns false and retains correlation. This differs from their initial overflow path. |
| Successfully formatted terminal under output backpressure | `emit` retains one complete line and reports transfer. `serviceOutput` clears its correlation only after the sink accepts the complete line. |
| Human renderer failure | Console emits its existing fallback text through the same terminal delivery path. |

Report wrappers recheck `item.operationId` after formatting: immediate emission
may already have cleared that outstanding slot. The HOME extraction keeps this
check, serial append, terminal operation selection and `emit` in Console.
Inspection emits with terminal operation zero and never consumes the host's
retained result. No uncertain write is replayed, and slow output does not read
or consume the motor UART.

## Extracted scope and source accounting

`HomeConsoleParser.h/.cpp` owns only HOME numeric/argument grammar.
`HomeConsoleFormat.h/.cpp` owns the method listing, homing JSON body and compact
eight-observation evidence serialization. The parser and formatter are separate
translation units with concrete inputs; neither imports complete Console state.
Console keeps the dispatch path and the single command catalogue.

Native test CMake, all inherited Arduino source filters, the native-IDF
application and portable-owner consumer explicitly compile both production
translation units. The coverage checker continues to read its real catalogue
from `ProbeConsole.cpp`. Its previously returned full source text was unused;
removing that plumbing changes no check. Catalogue operation prefixes and
public-header/evidence checks remain intact; executing the real console fixtures
and Python parity suites checks grammar and formatter behavior. No duplicate
registry or speculative source-text grammar checker is introduced.

The moved symbols are `HomeConsole::parse`, `HomeConsole::formatMethods` and
`HomeConsole::formatResult`; the compact evidence helper stays private to the formatter.
`ConsoleNumbers.h` shares the original checked unsigned-decimal parser, and
`ConsoleReport.h` shares the original bounded append, boolean, execution/outcome
names and hex helpers. Each has one implementation; other families keep using it. `Console::formatHome` remains a small
serial-append/emission wrapper, and `Console::reportHome` keeps correlation and
terminal delivery checks.

## FieldCore reference and deliberate differences

FieldCore was inspected read-only at
`c591ecddcd28ad3965c89a60b37279aa7f9e9940`: `CliParser.cpp` owns bounded
parsing, `CliRegistry.cpp` owns its catalogue/dispatch, `CliDeviceResultFormat.cpp`
owns scalar presentation, `CliPendingResults.cpp` owns pending owner-result
bookkeeping, and `SerialCli.cpp` owns bounded serial output orchestration.
`owners/Rs485Cli.cpp` submits owner requests and records pending correlation.

This console deliberately retains framework-neutral C++11 callbacks, fixed
caller-owned storage, protocol-3 JSONL and complete-line sink acceptance.
FieldCore's `Print`, global pending allocation, product presentation tokens,
measurement-only device request/result types and pending-expiry policies are
not imported. Its application owner remains a read-only integration reference;
motor admission and uncertainty remain with this repository's production API
and standalone owner.

## Remaining dependencies

The parser depends on the existing Homing/Rational types, checked exact-number
conversion, shared unsigned-decimal parsing and literal comparisons. The
formatter depends on Homing contexts/method descriptors, core error names and
shared bounded report helpers. Neither imports Console, invokes a callback,
reads time or retains state.

Other command families and large evidence formatters remain in Console.
Their distinct formatting failure policies are documented above, not normalized
by this file move. Further family extraction is optional work requiring its own
characterization. Prompt 06 still owns private application-fragment dependencies
and ordinary translation units; prompt 07 still owns concrete transport-evidence
copying. Host settings validators remain a separate group-policy task. No native
command gap, physical homing qualification or electrical timing gate is closed.

## Verification and measurements

The checker-only changes passed all **29 operation-inventory tests** and
`scripts/check_ess_operations.py`; counts remain 221 records, 49 operations,
135 named choices, nine gaps and 136 classified public functions.

Complete baseline/current comparisons used identical saved fixture source compiled
against the original production implementation and the extracted implementation.
All **681 emitted probe-console records (319,388 bytes)** and complete human
console output (**52,019 bytes**) match byte for byte. All seven existing
console/Python parity fixture families (**162 records**), including the 15 HOME
terminal cases, match. Added HOME boundary characterization (**13 records,
7,940 bytes**) also matches; it includes malformed first-error order, each missing
hook, rejected-ID reuse, maximum-width terminal/inspection and pending evidence.
No snapshot framework was added. Parser lexical/range and unchanged-output
checks, formatter zero/undersized/exact-capacity checks with canaries and existing
backpressure/immediate-delivery tests pass. Legitimate bounded HOME evidence
cannot overflow Console's full 8192-byte output buffer; helper failure is tested
with smaller explicit capacities, while Console's failure/correlation policy is
verified against unchanged wrapper source. No formatting-error behavior changes
were made.

Fresh `scripts/verify.py --mode quick --build-dir build/console-boundaries-quick`
passed all **77 registered native/Python tests**, generated inventory/metadata,
offline references and strict source/installed package consumers. This includes
all requested console and full-application fixtures, host motion/scenario tests,
seven real console/Python parity suites and operation inventory failure tests.
All four pinned Arduino builds passed.
`verify.py --mode full --firmware idf --build-dir build/console-boundaries-idf`
with pinned IDF 5.5.5 passed another full quick run plus the S3 application,
core S3/S2 consumers and portable S2 owner/console consumer. Exact pushed-commit CI is checked after delivery commits;
the final delivery records its commit and workflow run.

`sizeof(Console)` is unchanged: **18,944 bytes native; 18,496 bytes ESP32-S3**.
GNU ELF section measurements follow (before -> after bytes):

| Arduino environment | text | data | bss |
| --- | ---: | ---: | ---: |
| units | 231799 -> 231799 | 98637 -> 98637 | 587800 -> 587800 |
| probe | 425951 -> 426235 | 170259 -> 170279 | 1039582 -> 1039582 |
| load_poll | 428727 -> 428987 | 175211 -> 175215 | 1039898 -> 1039898 |
| load_timer | 428751 -> 429027 | 175227 -> 175231 | 1039898 -> 1039898 |

These are build/storage measurements, with no memory-optimization claim.
Hardware **NOT RUN**; no firmware flash or serial port is needed for this
behavior-preserving extraction.
