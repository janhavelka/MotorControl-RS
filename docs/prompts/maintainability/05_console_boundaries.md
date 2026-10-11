# 05: separate console grammar and bounded reporting

Read [execution rules](README.md). Inspect
[ProbeConsole.cpp](../../../examples/probe_cli/ProbeConsole.cpp), its header,
`ConsoleText.cpp`, `AxisConsole.h`, host callbacks and protocol-3/parity tests.
Inspect `check_ess_operations.py` before relocating command source. Recheck
FieldCore CLI/presenter ownership read-only, without importing it.

`Console::dispatch` combines token grammar, typed request construction, callback
availability/admission and reporting. Other methods serialize large evidence
objects in the same translation unit. The small reserved-slot selector is
already shared by admission and `track`; preserve it rather than duplicating it.

## Implement

1. Map grammar, pure parsing, callback dispatch, correlation, output queue and
   formatting responsibilities. Keep one command catalogue for names, syntax,
   effects and coverage. Identify a bounded parser or formatter that can move
   without importing the complete console state.
2. Extract one family into a private header/translation unit with explicit
   inputs and output capacity. Parsing should produce the existing typed
   request and have no motor side effects. Formatting borrows evidence only
   during the call; no retained pointers or extra unbounded strings.
3. Keep command IDs, human/JSON selection, outstanding records, reserved stop
   capacity, `beginWork`, error/admission order and emission ownership inside
   `Console`. Retain one dispatch path and one implementation of each grammar.
4. Move formatting separately from parsing so a mismatch has a small cause.
   Preserve protocol-3 field names/types, output order where observable,
   truncation failures and terminal delivery bookkeeping. Avoid a generic JSON
   DOM or template command framework just to reduce source length.
5. Update CMake, PlatformIO, native-IDF and actual test source lists together.
   Adjust source-accounting paths without dropping a command/operation check.

Document formatting outcomes before moving them. Some initial overflows emit
a terminal formatting-error line and untrack; later serial-field append failure
returns false and retains correlation. These are not interchangeable with output
backpressure. Report wrappers also recheck the operation ID after `emit`, since
immediate delivery may already clear the slot. Preserve those distinctions in
this extraction. Any later change to formatting-error behavior needs its own
focused tests and explicit behavior-change review; it is not a file move.

## Acceptance

Run `human_console`, `console_text`, `probe_console`, `host_console`,
`communication_console`, `persistence_console`, `discovery_console`, affected
full application fixtures, operation inventory and all real console/Python
parity suites. Exercise malformed/excess input,
missing hooks, cached health, full ordinary capacity plus reserved stop,
asynchronous/interleaved results and output backpressure/formatting failure.
Compare complete existing fixture output for each moved family before/after,
including malformed first-error and maximum-width evidence, without creating
a new snapshot framework or relying only on success/substrings.

Public syntax and JSONL remain unchanged; invalid commands invoke no motor
callback. Slow output must not consume bus bytes or block owner settlement.
Build both firmware consumers, complete quick verification and exact-commit CI.
Record remaining dependencies rather than forcing every command into one new
abstraction in this prompt.
