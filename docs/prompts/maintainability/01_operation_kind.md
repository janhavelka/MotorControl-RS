# 01: make application record selection explicit

Read [execution and delivery rules](README.md). This is a new implementation
task; the complexity audit only simplified local settings/console helpers.

## Inspect

In [ProbeApp.cpp](../../../examples/probe_cli/ProbeApp.cpp), inspect `App::Record`,
`terminal`, all admission functions, `actionEvent`, operation advancement,
`cancel`, `lookup`, `deliver` and result recycling. Include the session helpers
that create/inspect records and tests that include the real application source.
Inspect current FieldCore device request/result binding and owner read-only.

The six flags `typedRead`, `actionOperation`, `moveOperation`,
`velocityOperation`, `driverOperation` and `homeOperation` select mutually
exclusive contexts through repeated precedence chains. Other flags describe
orthogonal lifecycle/ownership facts and must not be folded into that selection.

## Implement in bounded steps

1. Inventory every valid record role and admission site. Include ordinary
   probe/capture, typed monitor reads, simple-motion children, programmatic
   moves, urgent stops and empty slots. Recovery, discovery and commissioning
   also have state outside `Record`; do not force them into the same enum.
2. Add one private operation-kind enum and replace the six selection flags.
   Establish exactly one kind on admission and reset/reuse. Retain all concrete
   contexts and the existing in-place reset to avoid large stack temporaries.
3. Replace selection chains with direct switches or short named selectors.
   Preserve each operation's evidence, cancellation and terminal policy.
   Keep `monitored`, `interactive`, `simpleChild`, `programmaticMove`, stop
   interruption, delivery and physical uncertainty as separate facts where
   their states are independent of kind.
4. Remove the old selectors only after every reader and writer is migrated.
   Do not introduce a union, virtual operation base, generic context container
   or public API change. Do not conflate a free record with a released axis.

## Acceptance

Exercise failed preparation and reuse of its slot; each admitted kind;
monitor/ordinary quotas; interrupted motion with a separately retained stop;
undelivered/result-pressure cases; release after uncertainty; stale generation;
discovery stop restoration; explicit recovery; Arduino/IDF simple-motion parity.

Use `probe_app`, `probe_app_monitor_control`, `move_app`, `simple_motion_app`,
`velocity_app`, `home_app`, `driver_app`, `discovery_app`, commissioning fixtures
and their IDF variants. Add only missing lifecycle cases revealed by the role
inventory. Requests/results, reservation accounting and service order must
match the baseline. Complete the common build/CI gates and report storage size
before/after without making a memory-optimization claim.
