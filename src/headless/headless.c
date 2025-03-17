#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <dlfcn.h>
#include <wchar.h>
#include <pwd.h>
#include <stdatomic.h>

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
#include <86box/nvr.h>
#include <86box/video.h>
#include <86box/ui.h>
#include <86box/gdbstub.h>

int             rctrl_is_lalt;


plat_joystick_t plat_joystick_state[MAX_PLAT_JOYSTICKS];
joystick_t      joystick_state[GAMEPORT_MAX][MAX_JOYSTICKS];
int             joysticks_present;

static int      exit_event         = 0;
static int      fullscreen_pending = 0;

uint32_t        lang_id = 0x0409, lang_sys = 0x0409; // Multilangual UI variables, for now all set to LCID of en-US
char            icon_set[256] = "";                  /* name of the iconset to be used */

mutex_t* blit_mutex;


// Starts the RAMP server and blocks the main thread.
void ramp_server_start(void);
void ramp_server_stop(void);


wchar_t *
plat_get_string(int i)
{
    switch (i) {
        case STRING_MOUSE_CAPTURE:
            return L"Click to capture mouse";
        case STRING_MOUSE_RELEASE:
            return L"Press CTRL-END to release mouse";
        case STRING_MOUSE_RELEASE_MMB:
            return L"Press CTRL-END or middle button to release mouse";
        case STRING_INVALID_CONFIG:
            return L"Invalid configuration";
        case STRING_NO_ST506_ESDI_CDROM:
            return L"MFM/RLL or ESDI CD-ROM drives never existed";
        case STRING_PCAP_ERROR_NO_DEVICES:
            return L"No PCap devices found";
        case STRING_PCAP_ERROR_INVALID_DEVICE:
            return L"Invalid PCap device";
        case STRING_GHOSTSCRIPT_ERROR_DESC:
            return L"libgs is required for automatic conversion of PostScript files to PDF.\n\nAny documents sent to the generic PostScript printer will be saved as PostScript (.ps) files.";
        case STRING_PCAP_ERROR_DESC:
            return L"Make sure libpcap is installed and that you are on a libpcap-compatible network connection.";
        case STRING_GHOSTSCRIPT_ERROR_TITLE:
            return L"Unable to initialize Ghostscript";
        case STRING_GHOSTPCL_ERROR_TITLE:
            return L"Unable to initialize GhostPCL";
        case STRING_GHOSTPCL_ERROR_DESC:
            return L"libgpcl6 is required for automatic conversion of PCL files to PDF.\n\nAny documents sent to the generic PCL printer will be saved as Printer Command Language (.pcl) files.";
        case STRING_HW_NOT_AVAILABLE_MACHINE:
            return L"Machine \"%hs\" is not available due to missing ROMs in the roms/machines directory. Switching to an available machine.";
        case STRING_HW_NOT_AVAILABLE_VIDEO:
            return L"Video card \"%hs\" is not available due to missing ROMs in the roms/video directory. Switching to an available video card.";
        case STRING_HW_NOT_AVAILABLE_TITLE:
            return L"Hardware not available";
        case STRING_MONITOR_SLEEP:
            return L"Monitor in sleep mode";
    }
    return L"";
}

// new source file TODO

void *
plat_mmap(size_t size, uint8_t executable)
{
#if defined __APPLE__ && defined MAP_JIT
    void *ret = mmap(0, size, PROT_READ | PROT_WRITE | (executable ? PROT_EXEC : 0), MAP_ANON | MAP_PRIVATE | (executable ? MAP_JIT : 0), -1, 0);
#else
    void *ret                    = mmap(0, size, PROT_READ | PROT_WRITE | (executable ? PROT_EXEC : 0), MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
    return (ret < 0) ? NULL : ret;
}

void
plat_munmap(void *ptr, size_t size)
{
    munmap(ptr, size);
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

    timer_freq = 1000000000LL;

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
}

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

char *
plat_vidapi_name(int i)
{
    return "ramp";
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
        ui_msgbox_header(MBX_FATAL, L"No ROMs found.", L"86Box could not find any usable ROM images.\n\nPlease download a ROM set and extract it into the \"roms\" directory.");
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

    return 0;
}

