// SPDX-License-Identifier: MIT
#pragma once
#include <cassert>
#include <vector>

// Include after ProbeApp.cpp. The caller owns fake UART setup and wiring.
// Only these supplied fixture facts qualify these scripted register replies.
namespace QualifiedInputFixture {
inline void service() { advanceHardware(hardware.time + 20); MotorControlRSExample::serviceApplication(); }
inline void read(MotorControlRS::ESS_RS::ReadKind kind,
                 const std::vector<std::vector<uint16_t>>& responses) {
    namespace ESS = MotorControlRS::ESS_RS;
    uint32_t id = 0;
    assert(host(app).startTypedRead(app, 9, 1, kind, id) == MotorControlRSExample::Probe::Action::OK);
    for (const auto& values : responses) {
        auto* record = findRecord(*app, id); const auto step = record->read.step;
        for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(record->requestId); ++i) service();
        assert(app->owner.txAccepted(record->requestId));
        std::vector<uint8_t> bytes = {1, 3, static_cast<uint8_t>(values.size() * 2)};
        for (uint16_t value : values) { bytes.push_back(static_cast<uint8_t>(value >> 8)); bytes.push_back(static_cast<uint8_t>(value)); }
        const uint16_t crc = ESS::calcCrc16(bytes.data(), bytes.size());
        bytes.push_back(static_cast<uint8_t>(crc)); bytes.push_back(static_cast<uint8_t>(crc >> 8));
        scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), bytes);
        for (unsigned i = 0; i < 25000 && record->read.step == step && record->read.state == MotorControlRS::ReadState::ACTIVE; ++i) service();
        assert(record->read.step != step || record->read.state != MotorControlRS::ReadState::ACTIVE);
    }
    assert(findRecord(*app,id)->read.state == MotorControlRS::ReadState::SUCCEEDED);
    for (unsigned i = 0; i < 100; ++i) service();
}
inline void install() {
    using namespace MotorControlRSExample;
    namespace ESS = MotorControlRS::ESS_RS;
    read(ESS::ReadKind::CONFIG, {{0,1000},{0,0,0},{1,0,0},{0,2,0,0,0},{2,4000}});
    read(ESS::ReadKind::STATE, {{0,1},{0,0},{0,0,0}});
    IoQualification q; assert(inspectIoQualification(1,0,IoPurpose::CONFIGURE,q));
    q.input.supplied = true; q.input.statusMapping = ESS::IoStatusMapping::AFTER_INVERSION;
    q.input.externalTriggersInhibited = true; assert(installIoQualification(q));
    bool valid = false; assert(copyIoQualification(1,0,IoPurpose::CONFIGURE,q,valid) && valid);
}
inline bool retained() {
    using namespace MotorControlRSExample;
    IoQualification q; bool valid = false;
    return copyIoQualification(1,0,IoPurpose::CONFIGURE,q,valid) && valid;
}
} // namespace QualifiedInputFixture
