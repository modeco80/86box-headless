/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Mononoke video backend.
 *
 *
 *
 * Authors: Lily Tsuru (modeco80)
 *      (c) 2023-2025 Lily Tsuru.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/video.h>
#include <86box/keyboard.h>
#include <86box/mouse.h>
#include <86box/plat.h>
#include <86box/ui.h>

#include "mononoke_server.hpp"

// This function runs on the blit thread.
static void
mononoke_video_blit(int x, int y, int w, int h, int monitor_index)
{
    mononoke::Server::the().Blit(x, y, w, h);

    // Signal to 86Box we have completed blit
    // and don't need access to the buffer anymore.
    video_blit_complete_monitor(monitor_index);
}

// These functions run on the main emulator thread.
// Thus, we can (probably) safely call 86Box routines in it.

extern "C" {
int
mononoke_video_init(UNUSED(void *arg))
{
    // Pause emulation
    plat_pause(1);
    cgapal_rebuild_monitor(0);

    /* Set up our BLIT handlers. */
    video_setblit(mononoke_video_blit);

    return (1);
}

void
mononoke_video_close(void)
{
    video_setblit(NULL);
}

void
mononoke_video_resize(int width, int height)
{
    //printf("mononoke_video_resize(%dx%d)\n", width, height);
    mononoke::Server::the().BlitResize(width, height);
}
}
