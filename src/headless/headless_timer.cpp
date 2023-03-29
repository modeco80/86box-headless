//
// Created by lily on 2/10/23.
//

#include <86box/plat.h>

#include <sys/time.h>
#include <ctime>

extern "C" {

// TODO: Expose these instead of having headless_timer.cpp hardcode everything :(
constexpr uint64_t NS_PER_MILLISECOND = 1000000;
constexpr uint64_t NS_PER_SECOND = 1000 * NS_PER_MILLISECOND;

// POSIX clock id
constexpr auto CLOCK_ID = CLOCK_MONOTONIC_RAW;

inline timespec plat_get_time() {
    timespec tv{};
    clock_gettime(CLOCK_ID, &tv);
    return tv;
}

uint64_t
plat_timer_read(void)
{
    auto tv = plat_get_time();
    return (tv.tv_sec * NS_PER_SECOND) + tv.tv_nsec;
}

inline uint64_t
plat_get_ticks_common(void)
{
    static bool firstUse = true;
    static uint64_t startTime;

    if (firstUse) {
        startTime = plat_timer_read();
        firstUse  = false;
    }

    return ((plat_timer_read() - startTime) * 1000000) / NS_PER_SECOND;
}

uint32_t
plat_get_ticks(void)
{
    return (uint32_t) (plat_get_ticks_common() / 1000);
}

uint32_t
plat_get_micro_ticks(void)
{
    return (uint32_t) plat_get_ticks_common();
}


void
plat_delay_ms(uint32_t count)
{
    timespec spec {
        .tv_nsec = static_cast<int64_t>(NS_PER_MILLISECOND * count)
    };

    clock_nanosleep(CLOCK_ID, 0, &spec, nullptr);
}

}