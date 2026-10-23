# 12 — Qualify the exact TL57 scope and close implementation handoffs

Follow the [execution contract](execution_contract.md). Audit all stage handoffs,
actual code/callers/tests and outstanding TL-D issues before choosing a test subset.

1. Reconcile every native operation/register family against callable typed API,
   CLI, failure tests and guards. Distinguish complete implementation from unresolved
   factory domains, external-trigger paths and unsupported model/revision features.
2. Verify the exact final software commit through local checks and required remote
   CI, including package and both framework consumers. Do not cite an older green
   commit or mark physical behavior qualified by parser tests.
3. Identify an actual authorized TL57 fixture, exact nameplate/options/firmware,
   serial tuple, I/O assignments, free-shaft/load state and stop/restoration contract.
   If unavailable, finish the software audit and record physical work as NOT RUN;
   never send TL57 commands to the recorded ESS bench by assumption.
4. Start with non-changing reads, then bounded explicit actions and small finite
   positive/negative position moves after prerequisites are established. Verify
   scale/reference and fresh completion evidence; retain raw traffic and settings.
   Run a short existing-function regression for every new image on the board.
5. Separate switched homing, output/brake loads, controlled restart/persistence,
   electrical timing, independent shaft measurements and broader stress into their
   actual fixture gates. Do not substitute unattended unbounded velocity or moving
   link-loss tests. A long soak requires its own authorized scope.
6. Reproduce failures deliberately, preserve competing-cause diagnostics, fix the
   established cause and rerun the original scenario plus regression. Unknown cause
   remains open, with the missing measurement named; never hide it with retries.

Publish a dated report and updated scope/backlog/roadmap: exact images/commits,
models, operations, limits, passes, failures, unrun checks and remaining issues.
Archive only completed prompt work. Release publication and FieldCore integration
remain subject to their existing workflows and separately authorized scope; no
automatic tag, release, broad family support or hardware-complete claim follows.
