// SPDX-License-Identifier: MIT
#pragma once
// Counts critical-section depth so the test can check that no SDK timer call
// runs inside the bench lock.
struct portMUX_TYPE { int depth; };
#define portMUX_INITIALIZER_UNLOCKED {0}
void fakeBenchEnter(portMUX_TYPE*);
void fakeBenchExit(portMUX_TYPE*);
#define portENTER_CRITICAL_SAFE(mux) fakeBenchEnter(mux)
#define portEXIT_CRITICAL_SAFE(mux) fakeBenchExit(mux)
