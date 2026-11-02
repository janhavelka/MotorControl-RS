# 09 - Qualify electrical timing and frame shapes

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q08, Q09, Q10**.

## Entry gate

An analyzer gates electrical claims only. Functional frame-shape and F9/E5 work may use software timing and passive capture, recorded as not electrically qualified. Keep the finite-position and velocity limits. Exact UART/transceiver/echo topology and supported firmware image; operation-specific prerequisites remain mandatory. Record SW1 (120 ohm; hardware p11 says the master and last slave need it), host termination and the RGND connection (p9) before attributing UART errors.

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
   obligation instead of claiming no prior hardware use. FM p11 documents FC03
   exception frames that change nothing: `01 03 00 1C 00 01 45 CC` gives code 03,
   `01 03 00 20 00 20 45 D8` gives code 05, `01 03 00 27 00 01 34 01` gives code
   06. These exercise the five-byte shape on the ordinary bench, through a bounded
   audited path. Never use the p11 `01 06 00 27 FF FF` example, because it is a
   motion-register write. No FC10 exception example exists.
4. Investigate malformed F9/E5 observations, actual UART error bits and
   mismatched tuples. Qualify alternate motor tuples, duplicate responders and
   any undocumented drive communication watchdog, if evidence shows one, only
   with their real fixtures and bounded access.
5. Correct demonstrable adapter defects at the existing boundary, preserving
   parser validation, source ambiguity and physical TX settlement.

## Acceptance and handoff

Exercise delayed service, partial TX, overflow/error, ambiguous echo, RX
truncation and deadline crossings, with traces linked to operation results
(analyzer traces for electrical claims). No raw write bypass to manufacture coverage. Preserve uncertainty when
the backend cannot prove closure or unique response attribution.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
