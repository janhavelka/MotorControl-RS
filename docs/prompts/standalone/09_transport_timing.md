# 09 - Qualify electrical timing and frame shapes

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q08, Q09, Q10**. Status: **NOT EXECUTED**.

## Entry gate

Analyzer, exact UART/transceiver/echo topology and supported firmware image; operation-specific prerequisites remain mandatory.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

examples/common/RtuRunner.cpp, RtuBusOwner.cpp, Esp32S3Uart.cpp; test/runner_test.cpp, bus_owner_test.cpp, esp32_uart_test.cpp, capture_service_test.cpp; docs/runner.md, bus_owner.md; archived ESS frame-size and host-tuple reports.

## Work

1. Correlate TX/RX/DE with final stop bit, TX drain publication, FIFO samples,
   earliest/latest closure bounds, idle watermarks, echo source and turnaround.
   A task timestamp or software fake is not a wire-time measurement.
2. Measure service/load tolerance and capture CPU/memory budget. Keep cache-off
   capture unsupported unless a separately justified implementation is qualified.
3. Complete physical shape/timing rows for five-byte exceptions and FC10
   0x0024/2, 0x001D/3 and 0x0031/6 (13/15/21-byte requests). A later functional
   selective-update pass for 0x0024/2 already exists; test its remaining timing
   obligation instead of claiming no prior hardware use.
4. Investigate malformed F9/E5 observations, actual UART error bits and
   mismatched tuples. Qualify alternate motor tuples, duplicate responders and
   watchdog effects only with their real fixtures and bounded access.
5. Correct demonstrable adapter defects at the existing boundary, preserving
   parser validation, source ambiguity and physical TX settlement.

## Acceptance and handoff

Exercise delayed service, partial TX, overflow/error, ambiguous echo, RX
truncation and deadline crossings, with analyzer traces linked to operation
results. No raw write bypass to manufacture coverage. Preserve uncertainty when
the backend cannot prove closure or unique response attribution.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
