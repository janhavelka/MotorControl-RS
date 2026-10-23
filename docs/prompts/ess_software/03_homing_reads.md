# 03 - Read the six native homing parameters

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Requirement: N02
`homing_parameter_reads`. Prerequisites: 01; reuse 02's agreed read conventions.

## Inspect and assign

Read `Reads.h/.cpp`, `Homing.h/.cpp`, generated ESS types, `ProbeApp.cpp`,
home console parser/formatter, read/home tests, the ledger and original function
manual pages 19-20 and 72-73. Lead owns shared read types/application integration.
Delegate independent seven-word decode/error tests after signatures freeze.
Use a read-only reviewer for offset and raw/qualified meaning.

## Work

1. Implement typed observation of auxiliary 0x0030, method 0x0031, search speed
   0x0032, query/return speed 0x0033, ramp 0x0034 and offset 0x0035/2. These are
   six records across seven words. Choose a reviewed bounded read plan; a single
   seven-word FC03 reply is 19 bytes. Audit helper capacities, prepared response
   expectations, retained evidence and owner storage rather than appending an
   enum to a five-word decoder.
2. Preserve every raw word, including both offset words and unknown method/
   auxiliary codes. Known enum membership is separate from executable method
   support. Do not reject a valid read solely because a returned code is unknown.
3. Keep unresolved offset order/sign/scale explicit. Reading WORD_ORDER does
   not by itself establish its applicability to this offset. Do not silently
   assemble a signed offset or translate rates/ramp into physical units.
   Zero raw offset is not proof of calibrated mechanical origin.
4. Add ordinary app/console read and result presentation through the same API.
   Show raw values and meaningful known/unsupported/unresolved states without
   suggesting that reading parameters has qualified or started a home.
5. Update six read obligations, API classification, route validation and
   maintained docs. Do not enable nonzero offsets, negative methods or a new
   write window; the existing FC10 0x0031/6 staging remains unchanged.

## Acceptance

Use seven distinct raw words so truncation, swapped offset words and wrong
field mapping fail visibly. Test unknown auxiliary/method, nonzero raw offset,
short/overlong replies, wrong same-length step, malformed frame/exception,
stale generation, immutable deadline, cancel and unchanged output on failure.
If reads span steps, test failure after the first and no partial publication or
atomic-snapshot claim. Prove no write or motion trigger is emitted.

Exercise real app and console paths, old homing behavior and invalid tokens.
Run focused read/home/parity/inventory checks and the required integrated
verification. Record new producing API evidence, remaining interpretation
limits and physical NOT RUN separately. No homing support flag changes here.

