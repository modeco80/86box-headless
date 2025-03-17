#include "highprec_timer.hpp"
#include <sys/time.h>
#include <ctime>

namespace util {

    // POSIX clock id
    constexpr auto CLOCK_ID = CLOCK_MONOTONIC_RAW;

    inline timespec GetClock() {
        timespec tv{};
        clock_gettime(CLOCK_ID, &tv);
        return tv;
    }

    uint64_t GetPerfCounter() {
        auto now = GetClock();

        uint64_t ticks = 0;
        ticks = now.tv_sec;
        ticks *= NS_PER_SECOND;
        ticks += now.tv_nsec;
        return ticks;
    }

    uint64_t GetPerfCounterFrequency() {
        return NS_PER_SECOND;
    }

    void DelayMs(uint64_t delay) {
        timespec spec {
            .tv_nsec = static_cast<int64_t>(NS_PER_MILLISECOND * delay)
        };
    
        clock_nanosleep(CLOCK_ID, 0, &spec, nullptr);
    }


}