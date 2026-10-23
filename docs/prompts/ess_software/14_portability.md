# 14 - Verify portable consumers, builds and storage bounds

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Prerequisite: 13.
Scope: prove the delivered features remain consumable on supported software
platforms; this is not a dependency upgrade or physical timing campaign.

## Inspect and assign

Read current CMake/package metadata, platform environments, pinned workflow,
verifier, portable/source/install consumers and changed public headers.
Lead owns build/registration changes and final integration. Delegate a disjoint
consumer fixture or read-only public-header audit. Spawn a read-only reviewer
for buffer/step/storage bounds and unintended platform/core dependencies.

## Work

1. Add focused consumer use of the new public reads, setters, interrupt intent
   and admitted homing API as needed to catch missing definitions, private-header
   dependencies and unusable contracts. Compile against both source and installed
   packages using the existing verifier fixtures.
2. Preserve strict C++11 public/core consumption, independent public headers,
   the existing C++17/no-RTTI consumer and codec-only symbol isolation.
   No platform types, hidden heap, clocks, transport or descriptive catalogue
   strings should become a new dependency of basic codecs.
3. Audit maximum requests/replies, seven-word reads, retained original/replacement/
   stop results, expanded homing phases, poll and step counters, application/
   owner object sizes and stack usage where measurable. Record actual deltas
   and bounds; do not guess MCU headroom from a host sizeof alone.
4. Build the supported Arduino and native-IDF matrix through current verifier
   commands/pins. Ensure all ordinary routes compile on both application frontends
   and S2/portable fixtures retain their stated scope.
5. Fix confirmed portability/registration/package defects without broadening
   platform support, updating SDKs or weakening warnings/checks as a workaround.
   Document required commands and any unavailable environment precisely.

## Acceptance

Fresh quick/source/install/header/symbol checks pass; the current supported
firmware matrix passes or is explicitly blocked with exact error evidence.
CI must verify the exact final implementation commit under current policy.
Check all tests are registered, nonempty and assertion-enabled.

Record artifact/summary paths and actual memory/size measurements. Builds do not
qualify S2 hardware, UART electrical timing, switch/index behavior or mechanical
motion. Leave those physical labels unchanged. Commit/push owned corrections
and hand concrete evidence to 15.

