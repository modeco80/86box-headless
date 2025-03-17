#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdint>

#include <86box/86box.h>

#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/keyboard.h>
#include <86box/mouse.h>
#include <86box/config.h>
#include <86box/path.h>
#include <86box/plat.h>
#include <86box/plat_dynld.h>
#include <86box/thread.h>
#include <86box/device.h>
#include <86box/gameport.h>
#include "cpu.h"
#include <86box/timer.h>
#include <86box/video.h>
#include <86box/ui.h>
#include <86box/gdbstub.h>

extern "C" {
#include <86box/nvr.h>
}

#include "util/highprec_timer.hpp"

extern "C" {

int rctrl_is_lalt;

plat_joystick_t plat_joystick_state[MAX_PLAT_JOYSTICKS];
joystick_t      joystick_state[GAMEPORT_MAX][MAX_JOYSTICKS];
int             joysticks_present;

static int exit_event         = 0;
static int fullscreen_pending = 0;

uint32_t lang_id = 0x0409, lang_sys = 0x0409; // Multilangual UI variables, for now all set to LCID of en-US
char     icon_set[256] = "";                  /* name of the iconset to be used */

mutex_t *blit_mutex;

// Starts the RAMP server and blocks the main thread.
void ramp_server_start(void);
void ramp_server_stop(void);

void
plat_power_off(void)
{
    confirm_exit = 0;
    nvr_save();
    config_save();

    /* Deduct a sufficiently large number of cycles that no instructions will
       run before the main thread is terminated */
    cycles -= 99999999;

    cpu_thread_run = 0;
}

void
plat_pause(int p)
{
    if ((p == 0) && (time_sync & TIME_SYNC_ENABLED))
        nvr_time_sync();

    dopause = p;
}

void
mouse_poll()
{
    // no-op; RAMP display assigns its own poll_ex handler
    // which is almost always called, but we still need
    // to define this because it's still referenced
}

void
set_language(uint32_t id)
{
    lang_id = id;
}

/* Sets up the program language before initialization. */
uint32_t
plat_language_code(char *langcode)
{
    /* or maybe not */
    return 0;
}

/* Converts back the language code to LCID */
void
plat_language_code_r(uint32_t lcid, char *outbuf, int len)
{
    /* or maybe not */
    return;
}
}

// TODO: A lockfree spsc queue here which is run on the emulator thread
// to call functions on it (to avoid any issues)

volatile int cpu_thread_run = 1;

void
main_thread(void *param)
{
    uint32_t old_time, new_time;
    int      drawits, frames;

    framecountx = 0;

    old_time = plat_get_ticks();
    drawits = frames = 0;

    while (!is_quit && cpu_thread_run) {
        /* See if it is time to run a frame of code. */
        new_time = plat_get_ticks();
#ifdef USE_GDBSTUB
        if (gdbstub_next_asap && (drawits <= 0))
            drawits = 10;
        else
#endif
            drawits += (new_time - old_time);
        old_time = new_time;

        if (drawits > 0 && !dopause) {
            /* Yes, so do one frame now. */
            drawits -= 10;
            if (drawits > 50)
                drawits = 0;

            /* Run a block of code. */
            pc_run();

            /* Every 200 frames we save the machine status. */
            if (++frames >= 200 && nvr_dosave) {
                nvr_save();
                nvr_dosave = 0;
                frames     = 0;
            }
        } else /* Just so we dont overload the host OS. */
            plat_delay_ms(1);

        /* If needed, handle a screen resize. */
        if (atomic_load(&doresize_monitors[0]) && !video_fullscreen && !is_quit) {
            // printf("resize to %d x %d\n", scrnsz_x, scrnsz_y);
            if (vid_resize & 2)
                plat_resize(fixed_size_x, fixed_size_y, 0);
            else
                plat_resize(scrnsz_x, scrnsz_y, 0);
            atomic_store(&doresize_monitors[0], 1);
        }
    }

    is_quit = 1;
}

thread_t *thMain = NULL;

void
do_start(void)
{
    /* We have not stopped yet. */
    is_quit = 0;

    timer_freq = util::GetPerfCounterFrequency();

    blit_mutex = thread_create_mutex();

    /* Start the emulator, really. */
    thMain = thread_create(main_thread, NULL);
}

void
do_stop(void)
{
    startblit();
    is_quit = 1;
    pc_close(NULL);

    // Stop the RAMP server
    ramp_server_stop();
    endblit();
}

void
joystick_init(void)
{
}

void
joystick_close(void)
{
}

void
joystick_process(void)
{
}

void
startblit(void)
{
    thread_wait_mutex(blit_mutex);
}

void
endblit(void)
{
    thread_release_mutex(blit_mutex);
}

int
main(int argc, char **argv)
{

    pc_init(argc, argv);
    if (!pc_init_modules()) {
        // this sucks but I don't make the rules.
        ui_msgbox_header(MBX_FATAL, (void *) L"No ROMs found.", (void *) L"86Box could not find any usable ROM images.\n\nPlease download a ROM set and extract it into the \"roms\" directory.");
        return 6;
    }

    /* Fire up the machine. */
    pc_reset_hard_init();

    /* Initialize the rendering window, or fullscreen. */
    do_start();

    // Unpause the emulated machine.
    plat_pause(0);

    // Start RAMP. This will block until the PC stops
    ramp_server_start();

    while (1)
        sleep(1);

    return 0;
}
