// SPDX-License-Identifier: MIT
// Input-start attribution core (wired bench 08). Every rule that admits an
// ATTRIBUTED outcome has a case here that turns INDETERMINATE (or another named
// outcome) when the witness is missing. Synthetic words only; no bench claim.
#include <MotorControlRS/profiles/ess_rs/InputStart.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace MotorControlRS;
using namespace MotorControlRS::ESS_RS;

namespace {
constexpr uint32_t OP = 41;
constexpr uint16_t STOPPED_WORD = 1, RUNNING_WORD = 4, HOMED_WORD = 3;
ReadTarget target() { ReadTarget t; t.id = t.address = 4; t.generation = 7; return t; }
InputStartPrerequisites facts(uint16_t function = 7, uint16_t mode = 0) {
    InputStartPrerequisites p;
    p.target = target(); p.configurationGeneration = 3; p.terminal = 3; p.function = function;
    p.positionModeKnown = true; p.positionMode = mode;
    p.pvTriggerModeKnown = true; p.pvTriggerMode = 0;
    p.sourceInhibitedUntilArmed = true; p.wordOrderKnown = true; p.wordOrder = WordOrder::HIGH_WORD_FIRST;
    p.baselineKnown = true; p.baselineUs = 100;
    const uint16_t words[7] = {0, STOPPED_WORD, 0, 0, 0, 500, 0};
    std::memcpy(p.baselineWords, words, sizeof(words));
    p.expectedKnown = true; p.expectedDisplacement = 1000; p.expectedEndpoint = 1500;
    return p;
}
InputStartOptions options() { InputStartOptions o; o.pollIntervalUs = 50000; o.windowUs = 10000000; return o; }
struct Run {
    InputStartContext c;
    uint64_t now = 1000;
    uint8_t frame[19] = {};
    explicit Run(const InputStartPrerequisites& p = facts(), const InputStartOptions& o = options()) {
        assert(prepareInputStart(c, p, OP, o, now));
    }
    void words(uint16_t motion, int32_t position, uint16_t inputs = 0, uint16_t alarm = 0, uint16_t speed = 0) {
        const uint16_t w[7] = {alarm, motion, inputs, 0, static_cast<uint16_t>(static_cast<uint32_t>(position) >> 16),
                               static_cast<uint16_t>(position), speed};
        frame[0] = 4; frame[1] = 3; frame[2] = 14;
        for (unsigned i = 0; i < 7; ++i) { frame[3 + 2 * i] = static_cast<uint8_t>(w[i] >> 8); frame[4 + 2 * i] = static_cast<uint8_t>(w[i]); }
        const uint16_t crc = calcCrc16(frame, 17);
        frame[17] = static_cast<uint8_t>(crc); frame[18] = static_cast<uint8_t>(crc >> 8);
    }
    InputStartEvent event(InputStartEventKind kind) {
        InputStartEvent e; e.kind = kind; e.operationId = OP; return e;
    }
    // One sample requested at now and answered 10 ms later.
    void sample(uint16_t motion, int32_t position, uint16_t inputs = 0, bool preEdge = false, uint16_t alarm = 0,
                uint64_t requestUs = 0, uint16_t speed = 0) {
        words(motion, position, inputs, alarm, speed);
        InputStartEvent e = event(InputStartEventKind::SAMPLE);
        e.sequence = preEdge ? 0 : c.nextSequence; e.preEdge = preEdge;
        e.requestUs = requestUs ? requestUs : now; now += 10000;
        e.transport.kind = ReadEventKind::FRAME; e.transport.target = target();
        e.transport.frame = frame; e.transport.length = sizeof(frame);
        e.transport.qualified = true; e.transport.earliestUs = now - 1000; e.transport.latestUs = now;
        assert(advanceInputStart(c, e, now));
    }
    void wait(uint64_t us) { now += us; }
    void lost() {
        InputStartEvent e = event(InputStartEventKind::LOST); e.sequence = c.nextSequence; e.requestUs = now;
        now += 10000; assert(advanceInputStart(c, e, now));
    }
    void isolation(bool held = true) { auto e = event(InputStartEventKind::ISOLATION); e.held = held; assert(advanceInputStart(c, e, now)); }
    void on() { assert(noteInputStartSource(c, now, true)); now += 1000; }
    void off() { assert(noteInputStartSource(c, now, false)); now += 1000; }
    void time() { assert(advanceInputStart(c, event(InputStartEventKind::TIME), now)); }
    // Post-release: stopped samples every 50 ms for 2.25 s.
    void settle(int32_t position) { for (unsigned i = 0; i < 45; ++i) { wait(40000); sample(STOPPED_WORD, position); } time(); }
    InputStartResult result() { InputStartResult r; assert(getInputStart(c, r)); return r; }
};
// The clean relative start used by most cases: baseline 500, staged +1000.
void armToEdge(Run& r, bool preEdge = true, bool held = true) {
    r.sample(STOPPED_WORD, 500); r.wait(40000);
    if (held) r.isolation(true); else r.isolation(false);
    if (preEdge) r.sample(STOPPED_WORD, 500, 0, true);
    r.on();
}
void moveAndRelease(Run& r, int32_t end = 1500) {
    r.wait(40000); r.sample(RUNNING_WORD, 900); r.wait(40000); r.sample(STOPPED_WORD, end);
    r.off();
}

void testPrepareRefusalsPreserveOutput() {
    const auto refuseWith = [](InputStartPrerequisites p, InputStartError error, InputStartOptions o) {
        InputStartContext c; c.operationId = 777;
        const auto status = prepareInputStart(c, p, OP, o, 1000);
        assert(!status && status.detail == static_cast<int32_t>(error));
        assert(c.operationId == 777 && c.state == ActionState::EMPTY);
    };
    const auto refuse = [&](const InputStartPrerequisites& p, InputStartError error) { refuseWith(p, error, options()); };
    auto p = facts(); p.function = 5; refuse(p, InputStartError::INVALID_FUNCTION);
    p = facts(); p.function = 14; refuse(p, InputStartError::INVALID_FUNCTION);
    // Wired bench 14: speed input and JOG (8, 9, 10) arm as velocity runs, without 0x0050 or 0x0051.
    for (uint16_t f : {8, 9, 10}) {
        p = facts(); p.function = f; p.positionModeKnown = p.pvTriggerModeKnown = false; p.expectedKnown = false;
        InputStartContext v; assert(prepareInputStart(v, p, OP, options(), 1000) && v.effect == InputStartEffect::VELOCITY_RUN);
    }
    p = facts(); p.positionModeKnown = false; refuse(p, InputStartError::POSITION_MODE_UNKNOWN);
    p = facts(); p.positionMode = 2; refuse(p, InputStartError::POSITION_MODE_UNKNOWN);
    p = facts(13); p.pvTriggerModeKnown = false; refuse(p, InputStartError::PV_TRIGGER_MODE_UNKNOWN);
    p = facts(); p.polarity = 1; refuse(p, InputStartError::MAPPING_UNKNOWN);
    p = facts(); p.sourceInhibitedUntilArmed = false; refuse(p, InputStartError::SOURCE_NOT_INHIBITED);
    p = facts(); p.baselineKnown = false; refuse(p, InputStartError::BASELINE_REQUIRED);
    p = facts(); p.baselineWords[0] = 5; refuse(p, InputStartError::ALARM);
    p = facts(); p.baselineWords[1] = 16; refuse(p, InputStartError::RELEASED);
    p = facts(); p.baselineWords[1] = RUNNING_WORD; refuse(p, InputStartError::NOT_STATIONARY);
    p = facts(); p.baselineWords[6] = 3; refuse(p, InputStartError::NOT_STATIONARY);
    p = facts(); p.baselineWords[2] = 8; refuse(p, InputStartError::INPUT_ACTIVE);
    p = facts(); p.maximumAgeUs = 500; refuse(p, InputStartError::STALE_FACTS);
    p = facts(); p.expectedKnown = false; refuse(p, InputStartError::EXPECTED_REQUIRED);
    p = facts(); p.terminal = 4; refuse(p, InputStartError::INVALID_TERMINAL);
    auto o = options(); o.pollIntervalUs = 19999; refuseWith(facts(), InputStartError::INVALID_OPTIONS, o);
    o = options(); o.windowUs = 30000001; refuseWith(facts(), InputStartError::INVALID_OPTIONS, o);
    o = options(); o.postReleaseSamples = 3; refuseWith(facts(), InputStartError::INVALID_OPTIONS, o);
    // An inverted polarity with a stated mapping is admitted; the level follows the mapping.
    p = facts(); p.polarity = 1; p.statusMapping = IoStatusMapping::AFTER_INVERSION;
    InputStartContext c; assert(prepareInputStart(c, p, OP, options(), 1000));
    p.statusMapping = IoStatusMapping::BEFORE_INVERSION; // Bit 0 with inverted polarity reads active.
    assert(!prepareInputStart(c, p, OP, options(), 1000));
    p.baselineWords[2] = 8; assert(prepareInputStart(c, p, OP, options(), 1000));
}
void testWorkIsReadOnlyAndRepeats() {
    Run r;
    PreparedInputStart w;
    assert(nextInputStart(r.c, r.now, w) && w.kind == InputStartWork::SAMPLE && w.sequence == 1);
    const uint8_t expected[6] = {4, 3, 0x00, 0x06, 0x00, 0x07};
    assert(w.length == 8 && !std::memcmp(w.bytes, expected, 6));
    PreparedInputStart again;
    assert(nextInputStart(r.c, r.now, again) && again.sequence == 1);
    r.sample(STOPPED_WORD, 500);
    assert(nextInputStart(r.c, r.now, w) && w.kind == InputStartWork::WAIT && w.eligibleUs == r.c.nextSampleUs);
    // A wrong sequence or operation leaves the context unchanged.
    InputStartEvent e; e.kind = InputStartEventKind::LOST; e.operationId = OP; e.sequence = 9; e.requestUs = r.now;
    const auto before = r.c.nextSequence;
    assert(!advanceInputStart(r.c, e, r.now) && r.c.nextSequence == before);
    e.sequence = before; e.operationId = 3;
    assert(!advanceInputStart(r.c, e, r.now) && r.c.lostSamples == 0);
    // One ON per arm; isolation only before it.
    r.isolation(); r.on();
    assert(!noteInputStartSource(r.c, r.now, true) && r.c.sourceOns == 1);
    InputStartEvent iso; iso.kind = InputStartEventKind::ISOLATION; iso.operationId = OP; iso.held = false;
    assert(!advanceInputStart(r.c, iso, r.now) && r.c.isolationHeld);
    InputStartEvent cancel; cancel.kind = InputStartEventKind::CANCEL; cancel.operationId = OP;
    assert(advanceInputStart(r.c, cancel, r.now) && r.c.outcome == InputStartOutcome::CANCELLED);
    assert(nextInputStart(r.c, r.now, w) && w.kind == InputStartWork::DONE);
    InputStartResult empty; empty.samples = 99; InputStartContext none;
    assert(!getInputStart(none, empty) && empty.samples == 99);
}
void testAttributedRelativeStart() {
    Run r; armToEdge(r);
    const uint64_t preEdgeRequest = r.c.preEdge.requestUs;
    moveAndRelease(r);
    r.time();
    assert(r.c.state == ActionState::ACTIVE); // Post-release window and samples not yet complete.
    r.settle(1500);
    const auto x = r.result();
    assert(x.outcome == InputStartOutcome::ATTRIBUTED && !x.missing);
    assert(x.execution == InputStartExecution::OBSERVED && x.completion == InputStartCompletion::FINITE_STOPPED);
    assert(x.further == InputStartFurther::NONE_SEEN && x.runningObserved && x.positionMatches && x.positionChange == 1000);
    assert(x.changeStartUs == preEdgeRequest && x.changeEndUs > r.c.onUs); // Bracket, not a start time.
    assert(x.spacingCount >= 2 && x.spacingMinUs && x.spacingMedianUs >= x.spacingMinUs && x.spacingMaxUs >= x.spacingMedianUs);
}
// Review A2-F8: a context cancelled before its post-release window cannot report
// that no further start was seen.
void testCancelBeforePostReleaseWindowIsNotNoneSeen() {
    Run r; armToEdge(r); moveAndRelease(r);
    assert(r.c.state == ActionState::ACTIVE && r.c.samplesAfterRelease < r.c.options.postReleaseSamples);
    assert(advanceInputStart(r.c, r.event(InputStartEventKind::CANCEL), r.now));
    const auto x = r.result();
    assert(x.outcome == InputStartOutcome::CANCELLED && x.further != InputStartFurther::NONE_SEEN);
}
void testFewerThanFourPostReleaseSamplesKeepObserving() {
    auto o = options(); o.postReleaseUs = 100000; // The time passes before four samples do.
    Run r(facts(), o); armToEdge(r); moveAndRelease(r);
    for (unsigned i = 0; i < 3; ++i) { r.wait(40000); r.sample(STOPPED_WORD, 1500); }
    r.time();
    assert(r.c.state == ActionState::ACTIVE && r.c.samplesAfterRelease == 3);
    r.wait(40000); r.sample(STOPPED_WORD, 1500); r.time();
    assert(r.c.outcome == InputStartOutcome::ATTRIBUTED);
}
// Each witness of the attribution rule, removed one at a time.
void testEachMissingWitnessIsIndeterminate() {
    const auto check = [](Run& r, uint16_t bit) {
        r.settle(1500);
        assert(r.c.outcome == InputStartOutcome::INDETERMINATE && (r.c.missing & bit));
    };
    { Run r; armToEdge(r, false); moveAndRelease(r); check(r, INPUT_START_MISSING_PRE_EDGE); }
    { Run r; r.isolation(); r.sample(STOPPED_WORD, 500, 8, true); r.on(); moveAndRelease(r);
      check(r, INPUT_START_MISSING_PRE_EDGE | INPUT_START_MISSING_INPUT_BEFORE_EDGE); }
    { Run r; armToEdge(r, true, false); moveAndRelease(r); check(r, INPUT_START_MISSING_ISOLATION); }
    { Run r; armToEdge(r); r.lost(); moveAndRelease(r); check(r, INPUT_START_MISSING_SAMPLE); }
    { Run r; armToEdge(r); r.wait(150000); moveAndRelease(r); check(r, INPUT_START_MISSING_SPACING); }
    for (uint16_t bit : {uint16_t(INPUT_START_MISSING_OVERWRITE), uint16_t(INPUT_START_MISSING_SERIAL_START),
                         uint16_t(INPUT_START_MISSING_GENERATION)}) {
        Run r; armToEdge(r);
        InputStartEvent e; e.kind = InputStartEventKind::DEGRADED; e.operationId = OP; e.missing = bit;
        assert(advanceInputStart(r.c, e, r.now));
        moveAndRelease(r); check(r, bit);
    }
    { // The first changed sample was requested before the noted edge.
        Run r; r.sample(STOPPED_WORD, 500); r.isolation(); r.sample(STOPPED_WORD, 500, 0, true);
        const uint64_t before = r.now; r.wait(30000); r.on();
        r.sample(RUNNING_WORD, 700, 0, false, 0, before + 1);
        r.wait(40000); r.sample(STOPPED_WORD, 1500); r.off();
        check(r, INPUT_START_MISSING_CHANGE_AFTER_EDGE);
    }
    { Run r; r.sample(STOPPED_WORD, 500); r.isolation(); r.sample(STOPPED_WORD, 500, 0, true);
      r.on(); r.off(); // Two edges cannot be noted again: a second ON is refused.
      assert(!noteInputStartSource(r.c, r.now, true) && r.c.sourceOns == 1); }
    // Every witness present: the same trace attributes (control).
    { Run r; armToEdge(r); moveAndRelease(r); r.settle(1500); assert(r.c.outcome == InputStartOutcome::ATTRIBUTED); }
}
void testMotionBeforeOrOutsideTheWindowIsUnattributed() {
    { Run r; r.sample(STOPPED_WORD, 500); r.sample(RUNNING_WORD, 520);
      assert(r.c.outcome == InputStartOutcome::UNATTRIBUTED_MOTION && r.c.state == ActionState::FAILED); }
    { Run r; r.isolation(); r.sample(STOPPED_WORD, 700, 0, true); assert(r.c.outcome == InputStartOutcome::UNATTRIBUTED_MOTION); }
    { // An edge noted after the window: every later change is unattributed.
        auto o = options(); o.windowUs = 100000; Run r(facts(), o);
        r.isolation(); r.sample(STOPPED_WORD, 500, 0, true); r.wait(200000);
        assert(noteInputStartSource(r.c, r.now, true) && r.c.sourceOutsideWindow);
        r.sample(RUNNING_WORD, 600);
        assert(r.c.outcome == InputStartOutcome::UNATTRIBUTED_MOTION);
    }
}
void testSecondStarts() {
    { Run r; armToEdge(r); r.wait(40000); r.sample(STOPPED_WORD, 1500); // Completed during the hold,
      r.wait(40000); r.sample(RUNNING_WORD, 1600);                       // then a second run while held.
      assert(r.c.outcome == InputStartOutcome::SECOND_START && r.result().further == InputStartFurther::SEEN); }
    { Run r; armToEdge(r); moveAndRelease(r); r.wait(600000); r.sample(STOPPED_WORD, 1500);
      r.wait(600000); r.sample(RUNNING_WORD, 1700); // After the falling edge, inside the post-release window.
      assert(r.c.outcome == InputStartOutcome::SECOND_START); }
    // A position change at standstill after the completion is not a start: it is
    // not excluded (settling or a start between samples), and never SECOND_START.
    { Run r; armToEdge(r); moveAndRelease(r); r.wait(40000); r.sample(STOPPED_WORD, 1500);
      r.wait(40000); r.sample(STOPPED_WORD, 2500); r.settle(2500);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::ATTRIBUTED && x.further == InputStartFurther::NOT_EXCLUDED);
      assert(x.positionMatches && x.positionChange == 2000); } // The completion matched; the later change is reported.
}
void testCompletionBetweenSamplesHasNoInventedOnset() {
    Run r; armToEdge(r);
    const uint64_t preEdgeRequest = r.c.preEdge.requestUs;
    r.wait(40000); r.sample(STOPPED_WORD, 500); const uint64_t lastUnchanged = r.now - 10000;
    r.wait(40000); r.sample(STOPPED_WORD, 1499); r.off(); r.settle(1499);
    const auto x = r.result();
    assert(x.outcome == InputStartOutcome::ATTRIBUTED && !x.runningObserved);
    assert(x.changeStartUs == lastUnchanged && x.changeStartUs > preEdgeRequest);
    assert(x.completion == InputStartCompletion::FINITE_STOPPED);
    assert(x.positionChange == 999 && x.expectedChange == 1000 && !x.positionMatches); // Reported, not judged.
}
void testNoStartAndRepeatedEndpoint() {
    { Run r; armToEdge(r); r.wait(40000); r.sample(STOPPED_WORD, 500); r.off(); r.settle(500);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::NO_START && x.execution == InputStartExecution::NOT_OBSERVED); }
    { auto p = facts(7, 1); p.expectedEndpoint = 500; // Absolute target already reached.
      Run r(p); armToEdge(r); r.wait(40000); r.sample(STOPPED_WORD, 500); r.off(); r.settle(500);
      assert(r.c.outcome == InputStartOutcome::INDETERMINATE && (r.c.missing & INPUT_START_MISSING_RUNNING)); }
    { auto p = facts(7, 1); p.expectedEndpoint = 500;
      Run r(p); armToEdge(r); r.wait(40000); r.sample(RUNNING_WORD, 500); r.wait(40000); r.sample(STOPPED_WORD, 500);
      r.off(); r.settle(500);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::ATTRIBUTED && x.further == InputStartFurther::NOT_EXCLUDED); }
    { Run r(facts(7, 1)); armToEdge(r); moveAndRelease(r); r.settle(1500);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::ATTRIBUTED && x.expectedChange == 1000 && x.positionMatches);
      assert(x.further == InputStartFurther::NOT_EXCLUDED); }
}
void testStopFaultTransportDeadline() {
    { Run r; armToEdge(r); r.wait(40000); r.sample(RUNNING_WORD, 900);
      InputStartEvent stop; stop.kind = InputStartEventKind::STOP; stop.operationId = OP; stop.stopId = 77;
      assert(advanceInputStart(r.c, stop, r.now)); r.off(); // The stop quiesced the held source.
      r.time(); assert(r.c.state == ActionState::ACTIVE); // Not at the stop acknowledgement.
      r.wait(40000); r.sample(STOPPED_WORD, 1100); r.settle(1100);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::STOPPED && x.stopId == 77 && x.stoppedAfterStop && x.execution == InputStartExecution::OBSERVED); }
    { Run r; armToEdge(r); r.wait(40000); r.sample(RUNNING_WORD, 900, 0, false, 5);
      assert(r.c.outcome == InputStartOutcome::DRIVE_FAULT); }
    { Run r; armToEdge(r);
      InputStartEvent fault; fault.kind = InputStartEventKind::FAULT; fault.operationId = OP;
      assert(advanceInputStart(r.c, fault, r.now) && r.c.outcome == InputStartOutcome::TRANSPORT_ERROR); }
    { auto o = options(); o.windowUs = 100000; Run r(facts(), o);
      r.sample(STOPPED_WORD, 500); r.wait(200000);
      PreparedInputStart w; assert(nextInputStart(r.c, r.now, w) && w.kind == InputStartWork::CONCLUDE);
      r.time(); assert(r.c.outcome == InputStartOutcome::DEADLINE && (r.c.missing & INPUT_START_MISSING_SOURCE_EDGE)); }
}
void testHomeAndVelocityEffects() {
    { Run r(facts(11)); armToEdge(r); r.wait(40000); r.sample(RUNNING_WORD, 300);
      r.wait(40000); r.sample(HOMED_WORD, 0); r.off(); r.settle(0);
      const auto x = r.result();
      assert(r.c.effect == InputStartEffect::HOME_COMPLETION && x.outcome == InputStartOutcome::ATTRIBUTED);
      assert(x.completion == InputStartCompletion::HOMED && !x.expectedKnown); }
    { // PV level trigger (0x0051 = 0): runs while held, stops after release.
      auto p = facts(13); p.pvTriggerMode = 0; Run r(p); armToEdge(r);
      r.wait(40000); r.sample(RUNNING_WORD, 800); r.wait(40000); r.sample(RUNNING_WORD, 1200); r.off();
      r.wait(40000); r.sample(STOPPED_WORD, 1300); r.settle(1300);
      const auto x = r.result();
      assert(r.c.effect == InputStartEffect::VELOCITY_RUN && x.outcome == InputStartOutcome::ATTRIBUTED);
      assert(x.completion == InputStartCompletion::VELOCITY_STOPPED && !x.expectedKnown && !x.positionMatches); }
    { // PV rising edge (0x0051 = 1): keeps running after release; only a stop concludes.
      auto p = facts(13); p.pvTriggerMode = 1; Run r(p); armToEdge(r);
      r.wait(40000); r.sample(RUNNING_WORD, 800); r.off();
      for (unsigned i = 0; i < 5; ++i) { r.wait(600000); r.sample(RUNNING_WORD, 900 + 100 * static_cast<int32_t>(i)); }
      r.time(); assert(r.c.state == ActionState::ACTIVE && r.result().completion == InputStartCompletion::NOT_OBSERVED);
      InputStartEvent stop; stop.kind = InputStartEventKind::STOP; stop.operationId = OP; stop.stopId = 5;
      assert(advanceInputStart(r.c, stop, r.now)); r.wait(40000); r.sample(STOPPED_WORD, 1500); r.settle(1500);
      assert(r.c.outcome == InputStartOutcome::STOPPED && r.result().completion == InputStartCompletion::VELOCITY_STOPPED); }
}
} // namespace

// Wired bench 09 (A6d, drive 4, stop normal ~1 s into an input-started +3000
// move, back-to-back reads): the drive cleared RUNNING while its speed word was
// still 23 and the position still moved (50734 running, 50763 not running at
// speed 23, 50764 at speed 0, then fixed). Completion marked at the first sample
// without RUNNING turned that last increment into SECOND_START (B2: 10 of 30
// member results). Standstill is RUNNING clear and speed word 0.
void testStopTailIsNotASecondStart() {
    const uint16_t SPEED_58 = 58, SPEED_23 = 23;
    auto recorded = facts(); recorded.baselineWords[4] = 0; recorded.baselineWords[5] = 49406; recorded.expectedDisplacement = 3000;
    { Run r(recorded); r.sample(STOPPED_WORD, 49406); r.wait(40000); r.isolation(true); r.sample(STOPPED_WORD, 49406, 0, true); r.on();
      r.wait(40000); r.sample(RUNNING_WORD, 50734, 0, false, 0, 0, SPEED_58); r.off();
      InputStartEvent stop; stop.kind = InputStartEventKind::STOP; stop.operationId = OP; stop.stopId = 91;
      assert(advanceInputStart(r.c, stop, r.now));
      r.wait(40000); r.sample(0, 50763, 0, false, 0, 0, SPEED_23);
      r.wait(40000); r.sample(0, 50764);
      r.settle(50764);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::STOPPED && x.further != InputStartFurther::SEEN && x.stoppedAfterStop);
      assert(x.completion == InputStartCompletion::FINITE_STOPPED && x.positionChange == 50764 - 49406); }
    // The same tail ended by a stop input (no serial stop): stopped short, attributed, never a second start.
    { Run r(recorded); r.sample(STOPPED_WORD, 49406); r.wait(40000); r.isolation(true); r.sample(STOPPED_WORD, 49406, 0, true); r.on();
      r.wait(40000); r.sample(RUNNING_WORD, 50734, 0, false, 0, 0, SPEED_58); r.off();
      r.wait(40000); r.sample(0, 50763, 0, false, 0, 0, SPEED_23);
      r.wait(40000); r.sample(0, 50764); r.settle(50764);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::ATTRIBUTED && x.further == InputStartFurther::NONE_SEEN && !x.positionMatches); }
    // Not running with a nonzero speed is not a conclusion either.
    { Run r; armToEdge(r); r.wait(40000); r.sample(RUNNING_WORD, 900); r.off();
      for (unsigned i = 0; i < 50; ++i) { r.wait(40000); r.sample(0, 1500, 0, false, 0, 0, 1); }
      r.time(); assert(r.c.state == ActionState::ACTIVE); }
    // B2 on the speed rule alone (drives 1 and 2): one increment after the reported
    // standstill. It is settling, reported as not excluded, never a second start.
    { Run r; armToEdge(r); r.wait(40000); r.sample(RUNNING_WORD, 900); r.wait(40000); r.sample(0, 1500);
      r.wait(40000); r.sample(0, 1501); r.off(); r.settle(1501);
      const auto x = r.result();
      assert(x.outcome == InputStartOutcome::ATTRIBUTED && x.further == InputStartFurther::NOT_EXCLUDED);
      assert(x.positionChange == 1001 && x.completion == InputStartCompletion::FINITE_STOPPED); }
    // RUNNING again after the standstill is a further start.
    { Run r; armToEdge(r); r.wait(40000); r.sample(RUNNING_WORD, 900); r.wait(40000); r.sample(0, 1500);
      r.wait(40000); r.sample(RUNNING_WORD, 1501);
      assert(r.c.outcome == InputStartOutcome::SECOND_START && r.result().further == InputStartFurther::SEEN); }
}
int main() {
    testStopTailIsNotASecondStart();
    testPrepareRefusalsPreserveOutput();
    testWorkIsReadOnlyAndRepeats();
    testAttributedRelativeStart();
    testCancelBeforePostReleaseWindowIsNotNoneSeen();
    testFewerThanFourPostReleaseSamplesKeepObserving();
    testEachMissingWitnessIsIndeterminate();
    testMotionBeforeOrOutsideTheWindowIsUnattributed();
    testSecondStarts();
    testCompletionBetweenSamplesHasNoInventedOnset();
    testNoStartAndRepeatedEndpoint();
    testStopFaultTransportDeadline();
    testHomeAndVelocityEffects();
    std::puts("input_start: PASS");
    return 0;
}
