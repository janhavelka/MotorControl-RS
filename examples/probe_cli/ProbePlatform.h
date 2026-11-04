// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>

namespace MotorControlRSExample { namespace Platform {
// Example-only console/task boundary. Exactly one cooperative application task
// calls these functions; motor and command semantics stay in ProbeApp.cpp.
bool beginConsole();
int availableConsole();
int readConsole();
int writableConsole();
std::size_t writeConsole(const uint8_t*, std::size_t);
void idle(unsigned milliseconds);
/// USB hardware state for diagnosing stalled output; all zero where unavailable.
struct ConsoleUsbSample {
    uint32_t interruptRaw = 0, interruptEnable = 0, inFifoState = 0, endpoint = 0;
    bool connected = false;
};
void sampleConsole(ConsoleUsbSample&);
/// Submits one USB IN packet if the FIFO reports space; true when it did.
bool nudgeConsole();
void bootFailure(const char* reason);
} }
