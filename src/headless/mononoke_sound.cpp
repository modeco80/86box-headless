/*
 * 86Box     A hypervisor and IBM PC system emulator that specializes in
 *           running old operating systems and software designed for IBM
 *           PC systems and compatibles from 1981 through fairly recent
 *           system designs based on the PCI bus.
 *
 *           This file is part of the 86Box distribution.
 *
 *           Audio interface for RAMP.
 *
 *
 *
 * Authors:  modeco80
 *
 *           Copyright 2023-2025 modeco80.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

extern "C" {
#include <86box/86box.h>
#include <86box/midi.h>
#include <86box/plat_dynld.h>
#include <86box/sound.h>
}

#include "mononoke_server.hpp"

static int                     midi_freq     = 44100;
static int                     midi_buf_size = 4410;
static int                     initialized   = 0;

#define FREQ   SOUND_FREQ
#define BUFLEN SOUNDBUFLEN

extern "C" {

void
inital(void)
{
    initialized = 1;
    atexit(closeal);
}

void
closeal(void)
{
    if (!initialized)
        return;
    initialized = 0;
}


void
al_set_midi(int freq, int buf_size)
{
    midi_freq     = freq;
    midi_buf_size = buf_size;
}


void
givealbuffer_common(const void *buf, const uint8_t src, const int size, const int freq)
{
    if (!initialized)
        return;
    // TODO: Pass data to a mixer, then once audio is mixed,
    // send it to Mononoke
}

void
givealbuffer(const void *buf)
{
    givealbuffer_common(buf, 0, BUFLEN << 1, FREQ);
}

void
givealbuffer_music(const void *buf)
{
    givealbuffer_common(buf, 1, MUSICBUFLEN << 1, MUSIC_FREQ);
}

void
givealbuffer_wt(const void *buf)
{
    givealbuffer_common(buf, 2, WTBUFLEN << 1, WT_FREQ);
}

void
givealbuffer_cd(const void *buf)
{
    givealbuffer_common(buf, 3, CD_BUFLEN << 1, CD_FREQ);
}

void
givealbuffer_midi(const void *buf, const uint32_t size)
{
    givealbuffer_common(buf, 4, (int) size, midi_freq);
}


}