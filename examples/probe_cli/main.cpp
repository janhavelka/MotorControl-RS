// SPDX-License-Identifier: MIT
#include "ProbeApp.h"
#include "../common/BoardPins.h"
#include <Arduino.h>
using namespace MotorControlRSExample;

// Bench, 16 November 2026: the group move path left 900 of the core's default
// 8192 loop-task bytes free. `memory` reports the remaining margin.
SET_LOOP_TASK_STACK_SIZE(12 * 1024);

void setup() {
    beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
                      Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit);
}
void loop() { serviceApplication(); }
