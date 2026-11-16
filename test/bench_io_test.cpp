// SPDX-License-Identifier: MIT
// Bench fixture outputs and input: the actual ESP32 adapter on fake GPIO,
// esp_timer and interrupt functions, and the console command routed to it.
#include "../examples/common/Esp32BenchIo.h"
#include "../examples/probe_cli/ProbeConsole.h"
#include <driver/gpio.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace Bench = MotorControlRSExample::BenchIo;
namespace Probe = MotorControlRSExample::Probe;
using MotorControlRSExample::Esp32BenchIo;

// ---- Fake SDK -------------------------------------------------------------
struct esp_timer {
    esp_timer_cb_t callback = nullptr;
    void* arg = nullptr;
    bool armed = false;
    uint64_t dueUs = 0;
};
namespace {
struct Sdk {
    uint64_t time = 1000;
    int lockDepth = 0;
    std::map<int, int> level;           ///< Driven output level or sensed input level.
    std::map<int, gpio_mode_t> mode;
    std::map<int, unsigned> writesBeforeConfig;
    std::vector<esp_timer*> timers;
    gpio_isr_t isr = nullptr;
    void* isrArg = nullptr;
    int isrPin = -1;
    unsigned isrInstalls = 0, gpioCalls = 0, timerStarts = 0, timerStops = 0;
    esp_err_t startResult = ESP_OK, configResult = ESP_OK, isrServiceResult = ESP_OK;
} sdk;
void resetSdk() {
    for (auto* timer : sdk.timers) delete timer;
    sdk = Sdk();
}
// Fire every armed timer whose due time has passed, as the esp_timer task would.
void advance(uint64_t us) {
    sdk.time += us;
    for (auto* timer : sdk.timers)
        if (timer->armed && timer->dueUs <= sdk.time) { timer->armed = false; timer->callback(timer->arg); }
}
void setInput(int pin, int value) {
    sdk.level[pin] = value;
    assert(sdk.isr && sdk.isrPin == pin);
    sdk.isr(sdk.isrArg);
}
}
void fakeBenchEnter(portMUX_TYPE* mux) { ++mux->depth; ++sdk.lockDepth; }
void fakeBenchExit(portMUX_TYPE* mux) { assert(mux->depth > 0); --mux->depth; --sdk.lockDepth; }
int64_t esp_timer_get_time() { return static_cast<int64_t>(sdk.time); }
esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out) {
    assert(sdk.lockDepth == 0 && args->callback && args->dispatch_method == ESP_TIMER_TASK);
    auto* timer = new esp_timer; timer->callback = args->callback; timer->arg = args->arg;
    sdk.timers.push_back(timer); *out = timer; return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t us) {
    assert(sdk.lockDepth == 0 && !timer->armed);
    ++sdk.timerStarts;
    if (sdk.startResult != ESP_OK) return sdk.startResult;
    timer->armed = true; timer->dueUs = sdk.time + us; return ESP_OK;
}
esp_err_t esp_timer_stop(esp_timer_handle_t timer) {
    assert(sdk.lockDepth == 0);
    ++sdk.timerStops;
    if (!timer->armed) return ESP_ERR_INVALID_STATE;
    timer->armed = false; return ESP_OK;
}
esp_err_t gpio_config(const gpio_config_t* config) {
    ++sdk.gpioCalls;
    assert(config->pull_up_en == GPIO_PULLUP_DISABLE && config->pull_down_en == GPIO_PULLDOWN_DISABLE);
    for (int pin = 0; pin < 64; ++pin) if (config->pin_bit_mask & (1ULL << pin)) {
        sdk.mode[pin] = config->mode;
        if (config->mode == GPIO_MODE_OUTPUT) assert(config->intr_type == GPIO_INTR_DISABLE);
        else assert(config->intr_type == GPIO_INTR_ANYEDGE);
    }
    return sdk.configResult;
}
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t value) {
    ++sdk.gpioCalls;
    if (!sdk.mode.count(pin)) ++sdk.writesBeforeConfig[pin];
    else assert(sdk.mode[pin] == GPIO_MODE_OUTPUT);
    sdk.level[pin] = static_cast<int>(value); return ESP_OK;
}
int gpio_get_level(gpio_num_t pin) { return sdk.level.count(pin) ? sdk.level[pin] : 1; }
esp_err_t gpio_install_isr_service(int flags) {
    assert(flags == 0); ++sdk.isrInstalls; return sdk.isrServiceResult;
}
esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t isr, void* arg) {
    sdk.isr = isr; sdk.isrArg = arg; sdk.isrPin = pin; return ESP_OK;
}

// ---- Helpers --------------------------------------------------------------
namespace {
constexpr int RELAY1 = 45, RELAY2 = 18, MOSFET1 = 3, MOSFET2 = 46, AUX1 = 17;
const int RESERVED[] = {47, 48, 21};
Bench::Pins boardPins() {
    Bench::Pins pins;
    pins.outputs[0].gpio = RELAY1; pins.outputs[1].gpio = RELAY2;
    pins.outputs[2].gpio = MOSFET1; pins.outputs[3].gpio = MOSFET2;
    pins.input.gpio = AUX1; pins.input.activeLow = true;
    return pins;
}
Bench::Request on(uint8_t output, uint32_t holdMs) {
    Bench::Request r; r.op = Bench::Op::ON; r.output = output; r.holdMs = holdMs; return r;
}
Bench::Request op(Bench::Op kind, uint8_t output = 0) {
    Bench::Request r; r.op = kind; r.output = output; return r;
}

void testPinValidation() {
    Bench::Pins pins = boardPins();
    assert(Bench::validPins(pins, RESERVED, 3));
    Bench::Pins none;
    assert(!Bench::validPins(none, RESERVED, 3)); // At least one pin.
    pins.outputs[1].gpio = RELAY1; assert(!Bench::validPins(pins, RESERVED, 3)); // Duplicate.
    pins = boardPins(); pins.outputs[0].gpio = 47; assert(!Bench::validPins(pins, RESERVED, 3)); // RS485 TX.
    pins = boardPins(); pins.input.gpio = 19; assert(!Bench::validPins(pins, RESERVED, 3)); // USB.
    pins = boardPins(); pins.outputs[3].gpio = 35; assert(!Bench::validPins(pins, RESERVED, 3)); // PSRAM.
    pins = boardPins(); pins.outputs[3].gpio = 23; assert(!Bench::validPins(pins, RESERVED, 3)); // Absent GPIO.
    pins = boardPins(); pins.outputs[3].gpio = -2; assert(!Bench::validPins(pins, RESERVED, 3));
    pins = boardPins(); pins.outputs[3].gpio = -1; assert(Bench::validPins(pins, RESERVED, 3)); // Absent output.
    assert(Bench::validHold(1) && Bench::validHold(Bench::MAX_HOLD_MS));
    assert(!Bench::validHold(0) && !Bench::validHold(Bench::MAX_HOLD_MS + 1));
}

void testBeginHoldsOutputsInactive() {
    resetSdk();
    Esp32BenchIo io;
    Bench::Pins bad = boardPins(); bad.outputs[0].gpio = 21;
    assert(!io.begin(bad, RESERVED, 3) && sdk.gpioCalls == 0); // Refused before any GPIO access.
    assert(io.begin(boardPins(), RESERVED, 3) && io.ready());
    for (int pin : {RELAY1, RELAY2, MOSFET1, MOSFET2}) {
        assert(sdk.writesBeforeConfig[pin] == 1); // Inactive level before the pad is an output.
        assert(sdk.mode[pin] == GPIO_MODE_OUTPUT && sdk.level[pin] == 0);
    }
    assert(sdk.mode[AUX1] == GPIO_MODE_INPUT && sdk.isrPin == AUX1 && sdk.isrInstalls == 1);
    assert(sdk.timers.size() == 4 && sdk.timerStarts == 0);
    assert(!io.begin(boardPins(), RESERVED, 3)); // One explicit startup.
    Bench::View view; assert(io.apply(Bench::Request(), view) == Bench::Result::OK);
    assert(view.ready && view.nowUs == sdk.time);
    for (const auto& output : view.outputs) assert(!output.active && output.offReason == Bench::OffReason::BOOT && !output.ons);
    assert(view.input.known && !view.input.active && view.input.edges == 0);

    // An already installed ISR service is accepted; another failure is not.
    resetSdk(); sdk.isrServiceResult = ESP_ERR_INVALID_STATE;
    Esp32BenchIo shared; assert(shared.begin(boardPins(), RESERVED, 3));
    resetSdk(); sdk.isrServiceResult = ESP_FAIL;
    Esp32BenchIo failed; assert(!failed.begin(boardPins(), RESERVED, 3) && !failed.ready());
    for (int pin : {RELAY1, RELAY2, MOSFET1, MOSFET2}) assert(sdk.level[pin] == 0);
    Bench::View unavailable;
    assert(failed.apply(on(0, 100), unavailable) == Bench::Result::UNAVAILABLE && !unavailable.ready);
    assert(sdk.level[RELAY1] == 0 && sdk.timerStarts == 0);
}

void testHoldTimerAndRenewal() {
    resetSdk();
    Esp32BenchIo io; assert(io.begin(boardPins(), RESERVED, 3));
    Bench::View view;
    const uint64_t start = sdk.time;
    assert(io.apply(on(2, 200), view) == Bench::Result::OK);
    assert(sdk.level[MOSFET1] == 1 && view.outputs[2].active && view.outputs[2].ons == 1);
    assert(view.outputs[2].onUs == start && view.outputs[2].dueUs == start + 200000 && view.outputs[2].holdMs == 200);
    assert(view.outputs[2].offReason == Bench::OffReason::NONE);
    advance(199999); assert(sdk.level[MOSFET1] == 1);
    advance(1); assert(sdk.level[MOSFET1] == 0);
    assert(io.apply(Bench::Request(), view) == Bench::Result::OK);
    assert(!view.outputs[2].active && view.outputs[2].offReason == Bench::OffReason::LEASE);
    assert(view.outputs[2].offUs == start + 200000 && view.outputs[2].dueUs == 0);

    // Renewal keeps the output ON, does not count a new ON and replaces the due time.
    assert(io.apply(on(0, 100), view) == Bench::Result::OK);
    const uint64_t firstDue = view.outputs[0].dueUs;
    advance(90000);
    assert(io.apply(on(0, 100), view) == Bench::Result::OK);
    assert(view.outputs[0].active && view.outputs[0].ons == 1 && view.outputs[0].dueUs == sdk.time + 100000);
    // A callback from the first ON that was already dispatched is ignored.
    sdk.time = firstDue; sdk.timers[0]->callback(sdk.timers[0]->arg);
    assert(sdk.level[RELAY1] == 1);
    advance(89999); assert(sdk.level[RELAY1] == 1);
    advance(1); assert(sdk.level[RELAY1] == 0);
    assert(io.apply(Bench::Request(), view) == Bench::Result::OK && view.outputs[0].offReason == Bench::OffReason::LEASE);
    assert(sdk.lockDepth == 0);
}

void testOffStopAndBackstop() {
    resetSdk();
    Esp32BenchIo io; assert(io.begin(boardPins(), RESERVED, 3));
    Bench::View view;
    assert(io.apply(on(0, 5000), view) == Bench::Result::OK && io.apply(on(3, 5000), view) == Bench::Result::OK);
    assert(io.apply(op(Bench::Op::OFF, 3), view) == Bench::Result::OK);
    assert(sdk.level[MOSFET2] == 0 && sdk.level[RELAY1] == 1 && !sdk.timers[3]->armed);
    assert(view.outputs[3].offReason == Bench::OffReason::REQUEST);
    io.quiesce(Bench::OffReason::STOP);
    assert(sdk.level[RELAY1] == 0 && !sdk.timers[0]->armed);
    assert(io.apply(Bench::Request(), view) == Bench::Result::OK);
    assert(view.outputs[0].offReason == Bench::OffReason::STOP && view.outputs[3].offReason == Bench::OffReason::REQUEST);
    // A stop keeps the reason of outputs that were already OFF.
    assert(view.outputs[1].offReason == Bench::OffReason::BOOT);

    assert(io.apply(on(1, 10), view) == Bench::Result::OK && io.apply(on(2, 10), view) == Bench::Result::OK);
    assert(io.apply(op(Bench::Op::OFF_ALL), view) == Bench::Result::OK);
    assert(!view.outputs[1].active && !view.outputs[2].active && view.outputs[1].offReason == Bench::OffReason::REQUEST);

    // Backstop: the service loop turns OFF an output whose timer did not fire.
    assert(io.apply(on(1, 10), view) == Bench::Result::OK);
    sdk.timers[1]->armed = false; // Lost timer event.
    sdk.time += 10000 + Bench::LATE_OFF_US - 1; io.service(sdk.time);
    assert(sdk.level[RELAY2] == 1);
    sdk.time += 1; io.service(sdk.time);
    assert(sdk.level[RELAY2] == 0);
    assert(io.apply(Bench::Request(), view) == Bench::Result::OK && view.outputs[1].offReason == Bench::OffReason::LATE);

    // A timer that cannot start leaves the output OFF.
    sdk.startResult = ESP_FAIL;
    assert(io.apply(on(2, 100), view) == Bench::Result::FAILED);
    assert(sdk.level[MOSFET1] == 0 && !view.outputs[2].active && view.outputs[2].offReason == Bench::OffReason::FAULT);
    sdk.startResult = ESP_OK;

    // Invalid requests change nothing.
    const unsigned starts = sdk.timerStarts;
    assert(io.apply(on(0, 0), view) == Bench::Result::INVALID);
    assert(io.apply(on(0, Bench::MAX_HOLD_MS + 1), view) == Bench::Result::INVALID);
    assert(io.apply(on(4, 10), view) == Bench::Result::INVALID);
    assert(sdk.timerStarts == starts && sdk.level[RELAY1] == 0);

    Esp32BenchIo partial; resetSdk();
    Bench::Pins pins = boardPins(); pins.outputs[3].gpio = -1;
    assert(partial.begin(pins, RESERVED, 3) && sdk.timers.size() == 3);
    assert(partial.apply(on(3, 10), view) == Bench::Result::INVALID && view.outputs[3].gpio == -1);
    assert(partial.apply(op(Bench::Op::OFF, 3), view) == Bench::Result::INVALID);
    partial.quiesce(Bench::OffReason::STOP);
    assert(sdk.lockDepth == 0);
}

void testInputEdges() {
    resetSdk();
    Esp32BenchIo io; assert(io.begin(boardPins(), RESERVED, 3));
    Bench::View view;
    sdk.time = 5000; setInput(AUX1, 0); // Energized input pulls the pin low.
    sdk.time = 6000; setInput(AUX1, 1);
    assert(io.apply(Bench::Request(), view) == Bench::Result::OK);
    assert(view.input.edges == 2 && view.input.recent == 2 && !view.input.active);
    assert(view.input.recentEdges[0].atUs == 5000 && view.input.recentEdges[0].active);
    assert(view.input.recentEdges[1].atUs == 6000 && !view.input.recentEdges[1].active);
    sdk.level[AUX1] = 0;
    assert(io.apply(Bench::Request(), view) == Bench::Result::OK && view.input.active);
    for (unsigned i = 0; i < 40; ++i) { sdk.time = 10000 + i; setInput(AUX1, static_cast<int>(i % 2)); }
    assert(io.apply(Bench::Request(), view) == Bench::Result::OK);
    assert(view.input.edges == 42 && view.input.overwritten == 42 - Bench::EDGE_CAPACITY);
    assert(view.input.recent == Bench::RECENT_EDGES);
    assert(view.input.recentEdges[0].atUs == 10000 + 40 - Bench::RECENT_EDGES);
    assert(view.input.recentEdges[Bench::RECENT_EDGES - 1].atUs == 10039);
    assert(io.apply(op(Bench::Op::CLEAR_EDGES), view) == Bench::Result::OK);
    assert(view.input.edges == 0 && view.input.recent == 0 && view.input.overwritten == 0);
    assert(sdk.lockDepth == 0);
}

// ---- Console route --------------------------------------------------------
struct ConsoleFake {
    Esp32BenchIo* io = nullptr;
    std::vector<std::string> lines;
    static bool emit(void* context, const char* text, std::size_t length) {
        static_cast<ConsoleFake*>(context)->lines.emplace_back(text, length); return true;
    }
    static Probe::Action bench(void* context, const Bench::Request* request, Bench::View& view) {
        auto& self = *static_cast<ConsoleFake*>(context);
        const Bench::Request show;
        const auto result = self.io->apply(request ? *request : show, view);
        return result == Bench::Result::OK ? Probe::Action::OK :
            result == Bench::Result::INVALID ? Probe::Action::INVALID : Probe::Action::UNAVAILABLE;
    }
    Probe::Host host(bool withBench) {
        Probe::Host h; h.context = this; h.emitLine = emit;
        if (withBench) h.benchIo = bench;
        return h;
    }
};
std::string send(Probe::Console& console, ConsoleFake& fake, const std::string& line) {
    fake.lines.clear();
    for (char c : line) console.feed(c);
    console.feed('\n');
    assert(fake.lines.size() == 1);
    return fake.lines.back();
}
bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

void testConsoleCommand() {
    resetSdk();
    Esp32BenchIo io; assert(io.begin(boardPins(), RESERVED, 3));
    ConsoleFake fake; fake.io = &io;
    Probe::Console console(fake.host(true));
    std::string reply = send(console, fake, "@1 bench");
    assert(has(reply, "\"command\":\"bench\",\"ok\":true,\"result\":\"done\",\"ready\":true"));
    assert(has(reply, "{\"name\":\"relay1\",\"gpio\":45,\"active\":false") && has(reply, "\"off_reason\":\"boot\""));
    assert(has(reply, "\"input\":{\"name\":\"aux1\",\"gpio\":17,\"known\":true,\"active\":false,\"edges\":0"));
    assert(has(reply, "\"bus_traffic\":false,\"motion_command\":false}"));
    reply = send(console, fake, "@2 bench on mosfet1 200");
    assert(has(reply, "\"ok\":true") && has(reply, "{\"name\":\"mosfet1\",\"gpio\":3,\"active\":true,\"hold_ms\":200,\"ons\":1"));
    assert(sdk.level[MOSFET1] == 1);
    reply = send(console, fake, "@3 bench off mosfet1");
    assert(sdk.level[MOSFET1] == 0 && has(reply, "\"off_reason\":\"request\""));
    Bench::View direct;
    assert(io.apply(on(0, 1000), direct) == Bench::Result::OK);
    send(console, fake, "@4 bench off");
    assert(sdk.level[RELAY1] == 0);
    sdk.time = 7000; setInput(AUX1, 0);
    reply = send(console, fake, "@5 bench");
    assert(has(reply, "\"active\":true,\"edges\":1,\"overwritten\":0,\"recent\":[[7000,true]]"));
    reply = send(console, fake, "@6 bench clear");
    assert(has(reply, "\"edges\":0") && has(reply, "\"recent\":[]"));
    for (const char* bad : {"@7 bench on mosfet1 0", "@8 bench on mosfet1 60001", "@9 bench on pump 10",
                            "@10 bench on mosfet1", "@11 bench off pump", "@12 bench toggle", "@13 bench clear now"}) {
        reply = send(console, fake, bad);
        assert(has(reply, "\"ok\":false,\"result\":\"invalid_arguments\""));
    }
    assert(sdk.level[MOSFET1] == 0 && sdk.level[RELAY1] == 0);
    reply = send(console, fake, "@14 help bench");
    assert(has(reply, "bench on relay1|relay2|mosfet1|mosfet2 HOLD_MS") && has(reply, "\"bus_traffic\":false"));
    // Human output renders through the generic JSON renderer.
    reply = send(console, fake, "bench");
    assert(!has(reply, "formatting failed") && has(reply, "relay1"));

    ConsoleFake absent;
    Probe::Console without(absent.host(false));
    reply = send(without, absent, "@1 bench on relay1 10");
    assert(has(reply, "\"result\":\"unavailable\""));
    reply = send(without, absent, "@2 help");
    assert(!has(reply, "\"bench\""));
}
}

int main() {
    testPinValidation();
    testBeginHoldsOutputsInactive();
    testHoldTimerAndRenewal();
    testOffStopAndBackstop();
    testInputEdges();
    testConsoleCommand();
    std::puts("bench_io: all tests passed");
    return 0;
}
