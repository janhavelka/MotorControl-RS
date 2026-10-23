# 11 - Implement origin plus positive-limit homing methods

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md), including stage 08's observation contract.
Requirement: N08 subset **7,8,9,10,23,24,25,26**. Prerequisite: 10.

## Inspect and assign

Read original physical PDF pages 37-43 and 55-61, every starting-location branch,
and current home core/app/tests. Lead owns family integration. Delegate the
independent branch/vector table and tests once contracts are fixed. Spawn a
read-only reviewer for far-side reversal, positive-limit mapping and false
completion from incomplete observations.

## Work

1. Review the source for each starting location: origin initially active,
   inactive on the approaching side, and inactive beyond the origin where the
   positive limit causes reversal. Record caption/figure discrepancies exactly.
   Do not reduce the method to a two-case active/inactive Boolean.
2. Require both origin and positive-limit functions with correct known polarity,
   declared wiring and relevant fresh logical state, even when a favorable
   start would not encounter the limit. Require internal Z qualification for
   7..10; omit it for 23..26.
3. Implement the eight native methods through shared home staging/trigger/
   observation. Preserve method-specific final edge/approach semantics under
   the reviewed observation contract; host software does not command the
   drive's intermediate reversals.
4. Keep unavailable/missed transitions explicit. A final zero or HOMED flag
   cannot be relabelled independent proof of every physical branch. Preserve
   bounded observation and qualified completion/reference meaning.
5. Integrate normal API/console routes and update only these method choices,
   descriptors and maintained documentation. Keep negative-limit family work
   for 12 and method 18 unresolved.

## Acceptance

Cover every method and documented starting-location branch using independently
derived expectations. Reject a missing positive limit even for an easy origin
path; test swapped limit assignment, opposite logical polarity, stale generations,
origin and limit observations from different configurations, and missing index
qualification where required.

Test stuck/missing/unobserved transitions, wrong or uncertain completion,
alarm/timeout/stop/cancel, final zero failure, late replies and lost staging/
trigger ACK without replay. Exercise real app/console paths, existing methods
and fixed buffer/poll bounds. Run focused and integrated checks/CI. Record eight
software dispositions; physical reversal/index evidence remains separately NOT RUN.

