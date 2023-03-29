/*
 * 86Box     A hypervisor and IBM PC system emulator that specializes in
 *           running old operating systems and software designed for IBM
 *           PC systems and compatibles from 1981 through fairly recent
 *           system designs based on the PCI bus.
 *
 *           This file is part of the 86Box distribution.
 *
 *           Null audio interface for headless port.
 *
 *
 *
 * Authors:  modeco80
 *
 *           Copyright 2023 modeco80.
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

static int                     midi_freq     = 44100;
static int                     midi_buf_size = 4410;
static int                     initialized   = 0;

#define FREQ   48000
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
givealbuffer(void *buf)
{
}

void
givealbuffer_cd(void *buf)
{
}

void
al_set_midi(int freq, int buf_size)
{
    midi_freq     = freq;
    midi_buf_size = buf_size;
}

void
givealbuffer_midi(void *buf, uint32_t size)
{
}

}