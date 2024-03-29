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
#include <rfb/rfb.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/video.h>
#include <86box/keyboard.h>
#include <86box/mouse.h>
#include <86box/plat.h>
#include <86box/ui.h>
#include <86box/vnc.h>

#define VNC_MIN_X 320
#define VNC_MAX_X 2048
#define VNC_MIN_Y 200
#define VNC_MAX_Y 2048

static rfbScreenInfoPtr rfb = NULL;
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
#define ENABLE_VNC_LOG 1

#ifdef ENABLE_VNC_LOG
int rampvideo_do_log = ENABLE_VNC_LOG;

static void
rampvideo_log(const char *fmt, ...)
{
    va_list ap;

    if (rampvideo_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define vnc_log(fmt, ...)
#endif


static void
rampvideo_blit(int x, int y, int w, int h, int monitor_index)
{
 
    //for (row = 0; row < h; ++row)
    //    video_copy(&(((uint8_t *) rfb->frameBuffer)[row * 2048 * sizeof(uint32_t)]), &(buffer32->line[y + row][x]), w * sizeof(uint32_t));


}

extern "C" {
int
rampvideo_init(UNUSED(void *arg))
{
    plat_pause(1);
    cgapal_rebuild_monitor(0);


    /* Set up our BLIT handlers. */
    video_setblit(rampvideo_blit);

    return (1);
}

void
rampvideo_close(void)
{
    video_setblit(NULL);
}

void
rampvideo_resize(int x, int y)
{
   // Need to handle this later :)
}

}

/*
void
rampvideo_take_screenshot(wchar_t *fn)
{
    rampvideo_log("VNC: take_screenshot\n");
}
*/
