# Native ESP-IDF standalone consumer

This ESP32-S3 application uses ESP-IDF 5.5.5. It compiles the same
`ProbeApp.cpp`, console, owner, UART/capture and library operations as Arduino;
framework startup and task adapters differ; both consumers use the same IDF
USB console driver.

See [build, board configuration, flash and recovery instructions](../../docs/esp_idf_probe.md)
and [native-IDF implementation verification](../../docs/reports/ess_release_25_2026-10-05.md).
Board pins and memory choices are example Kconfig/defaults, not public APIs.
The default workload is zero and startup emits no motor request.

Use the native `idf.py`/CMake build. Arduino is built through the existing root
PlatformIO environments. The [cross-platform report](../../docs/reports/ess_release_26_2026-10-05.md)
records finite motion, stop and load evidence on both S3 consumers. Later Arduino
position-boundary results are separate; see the [current release scope](../../docs/releases/1.0.0.md).
