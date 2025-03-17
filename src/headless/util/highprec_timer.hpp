#pragma once
#include <cstdint>

namespace util {

    // Fun constants
    constexpr uint64_t NS_PER_MILLISECOND = 1000000;
    constexpr uint64_t NS_PER_SECOND = 1000000000LL;

    /// Returns the frequency of the high precision timer.
    uint64_t GetPerfCounterFrequency();

    uint64_t GetPerfCounter();

    void DelayMs(uint64_t delay);

}