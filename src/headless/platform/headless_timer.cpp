//
// Created by lily on 2/10/23.
//

// This essentially wires up the highprec time provided by
// util/highprec_timer.cpp to the 86Box codebase.

#include <86box/plat.h>

#include "../util/highprec_timer.hpp"

static bool startTimeInitalized = false;
static uint64_t startTime = 0;
static uint64_t frequency = 0;

extern "C" {

uint64_t
plat_timer_read(void)
{
   return util::GetPerfCounter();
}

inline uint64_t
plat_get_ticks_common(void)
{
    if (startTimeInitalized == false) {
        startTimeInitalized = true;
        startTime = util::GetPerfCounter();
        frequency = util::GetPerfCounterFrequency();
    }

    return ((util::GetPerfCounter() - startTime) * 1000000) / frequency;
}

uint32_t
plat_get_ticks(void)
{
    return (uint32_t) (plat_get_ticks_common() / 1000);
}

void
plat_delay_ms(uint32_t count)
{
    util::DelayMs(count);
}

}