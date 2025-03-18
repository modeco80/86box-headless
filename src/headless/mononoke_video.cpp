/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implement the VNC remote renderer with LibVNCServer.
 *
 *
 *
 * Authors: Fred N. van Kempen, <decwiz@yahoo.com>
 *          Based on raw code by RichardG, <richardg867@gmail.com>
 *
 *          Copyright 2017-2019 Fred N. van Kempen.
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

static int              clients;
static int              updatingSize;
static int              allowedX,
    allowedY;
static int ptr_x, ptr_y, ptr_but;

typedef struct {
    int buttons;
    int dx;
    int dy;
    int dwheel;
} MOUSESTATE;

static MOUSESTATE ms;


static void
mononoke_video_blit(int x, int y, int w, int h, int monitor_index)
{
    printf("rampvideo_blit(rect: %dx%d at %dx%d, monitor %d)\n", w, h, x, y, monitor_index);
 
    //for (row = 0; row < h; ++row)
    //    video_copy(&(((uint8_t *) rfb->frameBuffer)[row * 2048 * sizeof(uint32_t)]), &(buffer32->line[y + row][x]), w * sizeof(uint32_t));


    // Signal to 86Box we don't need the buffer anymore.
    video_blit_complete_monitor(monitor_index);
}

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
mononoke_video_resize(int x, int y)
{
   // Need to handle this later :)
}

}

/*
void
mononoke_video_take_screenshot(wchar_t *fn)
{
    rampvideo_log("VNC: take_screenshot\n");
}
*/
