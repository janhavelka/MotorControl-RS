# 03 - Position interruption: implementation redirected

Finding N05 is owned by the dedicated ESS software promptset:

- [06 - Typed serial interrupt core contract](../ess_software/06_position_interrupt_core.md).
- [07 - Owner, application and console integration](../ess_software/07_position_interrupt_owner.md).

Read that set's [README](../ess_software/README.md), execution contract and
prerequisites before dispatch. It covers the source-reviewed bit-3 semantics,
ordinary API, independent old/replacement/stop results and no-replay behavior.
This redirect does not mean interruption is implemented. Physical interruption
qualification remains separate; do not implement the same feature twice.
