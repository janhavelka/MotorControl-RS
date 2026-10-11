// SPDX-License-Identifier: MIT
#pragma once
#include "Hardware.h"
#include <algorithm>
#include <limits>
#include <string>

struct FakeSerial {
    std::string input, output;
    std::size_t writeCapacity = 4096;
    std::size_t writeLimit = std::numeric_limits<std::size_t>::max();
    std::size_t writeCalls = 0, maxWriteSize = 0;
    uint32_t writeTimeUs = 0;
    bool consumeCapacity = false;
    void begin(unsigned) {}
    int available() const { return static_cast<int>(input.size()); }
    int read() { const char c = input.front(); input.erase(0, 1); return c; }
    std::size_t write(const uint8_t* data, std::size_t size) {
        ++writeCalls;
        maxWriteSize = std::max(maxWriteSize, size);
        const std::size_t accepted = std::min(size, std::min(writeCapacity, writeLimit));
        output.append(reinterpret_cast<const char*>(data), accepted);
        if (consumeCapacity) writeCapacity -= accepted;
        advanceHardware(hardware.time + writeTimeUs);
        return accepted;
    }
};
extern FakeSerial Serial;
inline void delay(unsigned ms) { advanceHardware(hardware.time + static_cast<uint64_t>(ms) * 1000); }
