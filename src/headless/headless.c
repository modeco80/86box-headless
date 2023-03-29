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
#include <86box/timer.h>
#include <86box/nvr.h>
#include <86box/video.h>
#include <86box/ui.h>
#include <86box/gdbstub.h>

int             rctrl_is_lalt;


plat_joystick_t plat_joystick_state[MAX_PLAT_JOYSTICKS];
joystick_t      joystick_state[MAX_JOYSTICKS];
int             joysticks_present;

static int      exit_event         = 0;
static int      fullscreen_pending = 0;

uint32_t        lang_id = 0x0409, lang_sys = 0x0409; // Multilangual UI variables, for now all set to LCID of en-US
char            icon_set[256] = "";                  /* name of the iconset to be used */

mutex_t* blit_mutex;

wchar_t *
plat_get_string(int i)
{
    switch (i) {
        case IDS_2077:
            return L"Click to capture mouse";
        case IDS_2078:
            return L"Press CTRL-END to release mouse";
        case IDS_2079:
            return L"Press CTRL-END or middle button to release mouse";
        case IDS_2080:
            return L"Failed to initialize FluidSynth";
        case IDS_2131:
            return L"Invalid configuration";
        case IDS_4099:
            return L"MFM/RLL or ESDI CD-ROM drives never existed";
        case IDS_2094:
            return L"Failed to set up PCap";
        case IDS_2095:
            return L"No PCap devices found";
        case IDS_2096:
            return L"Invalid PCap device";
        case IDS_2111:
            return L"Unable to initialize FreeType";
        case IDS_2112:
            return L"Unable to initialize SDL, libsdl2 is required";
        case IDS_2132:
            return L"libfreetype is required for ESC/P printer emulation.";
        case IDS_2133:
            return L"libgs is required for automatic conversion of PostScript files to PDF.\n\nAny documents sent to the generic PostScript printer will be saved as PostScript (.ps) files.";
        case IDS_2134:
            return L"libfluidsynth is required for FluidSynth MIDI output.";
        case IDS_2130:
            return L"Make sure libpcap is installed and that you are on a libpcap-compatible network connection.";
        case IDS_2115:
            return L"Unable to initialize Ghostscript";
        case IDS_2063:
            return L"Machine \"%hs\" is not available due to missing ROMs in the roms/machines directory. Switching to an available machine.";
        case IDS_2064:
            return L"Video card \"%hs\" is not available due to missing ROMs in the roms/video directory. Switching to an available video card.";
        case IDS_2129:
            return L"Hardware not available";
        case IDS_2143:
            return L"Monitor in sleep mode";
        default:
            return L"";
    }
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
                plat_resize(fixed_size_x, fixed_size_y);
            else
                plat_resize(scrnsz_x, scrnsz_y);
            atomic_store(&doresize_monitors[0], 1);
        }
    }

    is_quit = 1;
}

//thread_t *thMain = NULL;

void
do_start(void)
{
    /* We have not stopped yet. */
    is_quit = 0;

    timer_freq = 1000000000LL;

    blit_mutex = thread_create_mutex();

    /* Start the emulator, really. */
    //thMain = thread_create(main_thread, NULL);
}

void
do_stop(void)
{
    startblit();
    is_quit = 1;
    pc_close(NULL);
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
    // no-op; VNC display assigns its own poll_ex handler
    // which is almost always called, but we still need
    // to define this because it's still referenced
}

char *
plat_vidapi_name(int i)
{
    return "headless";
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

// headless_util.cpp
char *
local_strsep(char **str, const char *sep);

char *xargv[512];

bool
process_media_commands_3(uint8_t *id, char *fn, uint8_t *wp, int cmdargc)
{
    bool err = false;
    *id      = atoi(xargv[1]);
    if (xargv[2][0] == '\'' || xargv[2][0] == '"') {
        int curarg = 2;
        for (curarg = 2; curarg < cmdargc; curarg++) {
            if (strlen(fn) + strlen(xargv[curarg]) >= PATH_MAX) {
                err = true;
                fprintf(stderr, "Path name too long.\n");
            }
            strcat(fn, xargv[curarg] + (xargv[curarg][0] == '\'' || xargv[curarg][0] == '"'));
            if (fn[strlen(fn) - 1] == '\''
                || fn[strlen(fn) - 1] == '"') {
                if (curarg + 1 < cmdargc) {
                    *wp = atoi(xargv[curarg + 1]);
                }
                break;
            }
            strcat(fn, " ");
        }
    } else {
        if (strlen(xargv[2]) < PATH_MAX) {
            strcpy(fn, xargv[2]);
            *wp = atoi(xargv[3]);
        } else {
            fprintf(stderr, "Path name too long.\n");
            err = true;
        }
    }
    if (fn[strlen(fn) - 1] == '\''
        || fn[strlen(fn) - 1] == '"')
        fn[strlen(fn) - 1] = '\0';
    return err;
}

char *(*f_readline)(const char *)          = NULL;
int (*f_add_history)(const char *)         = NULL;
void (*f_rl_callback_handler_remove)(void) = NULL;

#ifdef __APPLE__
#    define LIBEDIT_LIBRARY "libedit.dylib"
#else
#    define LIBEDIT_LIBRARY "libedit.so"
#endif

void
monitor_thread(void *param)
{
#ifndef USE_CLI
    if (isatty(fileno(stdin)) && isatty(fileno(stdout))) {
        char  *line = NULL;
        size_t n;
        printf("86Box monitor console.\n");
        while (!exit_event) {
            if (feof(stdin))
                break;
            if (f_readline)
                line = f_readline("(86Box) ");
            else {
                printf("(86Box) ");
                getline(&line, &n, stdin);
            }
            if (line) {
                int   cmdargc = 0;
                char *linecpy;
                line[strcspn(line, "\r\n")] = '\0';
                linecpy                     = strdup(line);
                if (!linecpy) {
                    free(line);
                    line = NULL;
                    continue;
                }
                if (f_add_history)
                    f_add_history(line);
                memset(xargv, 0, sizeof(xargv));
                while (1) {
                    xargv[cmdargc++] = local_strsep(&linecpy, " ");
                    if (xargv[cmdargc - 1] == NULL || cmdargc >= 512)
                        break;
                }
                cmdargc--;
                if (strncasecmp(xargv[0], "help", 4) == 0) {
                    printf(
                        "fddload <id> <filename> <wp> - Load floppy disk image into drive <id>.\n"
                        "cdload <id> <filename> - Load CD-ROM image into drive <id>.\n"
                        "zipload <id> <filename> <wp> - Load ZIP image into ZIP drive <id>.\n"
                        "cartload <id> <filename> <wp> - Load cartridge image into cartridge drive <id>.\n"
                        "moload <id> <filename> <wp> - Load MO image into MO drive <id>.\n\n"
                        "fddeject <id> - eject disk from floppy drive <id>.\n"
                        "cdeject <id> - eject disc from CD-ROM drive <id>.\n"
                        "zipeject <id> - eject ZIP image from ZIP drive <id>.\n"
                        "carteject <id> - eject cartridge from drive <id>.\n"
                        "moeject <id> - eject image from MO drive <id>.\n\n"
                        "hardreset - hard reset the emulated system.\n"
                        "pause - pause the the emulated system.\n"
                        "fullscreen - toggle fullscreen.\n"
                        "exit - exit 86Box.\n");
                } else if (strncasecmp(xargv[0], "exit", 4) == 0) {
                    exit_event = 1;
                } else if (strncasecmp(xargv[0], "fullscreen", 10) == 0) {
                    video_fullscreen   = video_fullscreen ? 0 : 1;
                    fullscreen_pending = 1;
                } else if (strncasecmp(xargv[0], "pause", 5) == 0) {
                    plat_pause(dopause ^ 1);
                    printf("%s", dopause ? "Paused.\n" : "Unpaused.\n");
                } else if (strncasecmp(xargv[0], "hardreset", 9) == 0) {
                    pc_reset_hard();
                } else if (strncasecmp(xargv[0], "cdload", 6) == 0 && cmdargc >= 3) {
                    uint8_t id;
                    bool    err = false;
                    char    fn[PATH_MAX];

                    if (!xargv[2] || !xargv[1]) {
                        free(line);
                        free(linecpy);
                        line = NULL;
                        continue;
                    }
                    id = atoi(xargv[1]);
                    memset(fn, 0, sizeof(fn));
                    if (xargv[2][0] == '\'' || xargv[2][0] == '"') {
                        int curarg = 2;
                        for (curarg = 2; curarg < cmdargc; curarg++) {
                            if (strlen(fn) + strlen(xargv[curarg]) >= PATH_MAX) {
                                err = true;
                                fprintf(stderr, "Path name too long.\n");
                            }
                            strcat(fn, xargv[curarg] + (xargv[curarg][0] == '\'' || xargv[curarg][0] == '"'));
                            if (fn[strlen(fn) - 1] == '\''
                                || fn[strlen(fn) - 1] == '"') {
                                break;
                            }
                            strcat(fn, " ");
                        }
                    } else {
                        if (strlen(xargv[2]) < PATH_MAX) {
                            strcpy(fn, xargv[2]);
                        } else {
                            fprintf(stderr, "Path name too long.\n");
                        }
                    }
                    if (!err) {

                        if (fn[strlen(fn) - 1] == '\''
                            || fn[strlen(fn) - 1] == '"')
                            fn[strlen(fn) - 1] = '\0';
                        printf("Inserting disc into CD-ROM drive %hhu: %s\n", id, fn);
                        cdrom_mount(id, fn);
                    }
                } else if (strncasecmp(xargv[0], "fddeject", 8) == 0 && cmdargc >= 2) {
                    floppy_eject(atoi(xargv[1]));
                } else if (strncasecmp(xargv[0], "cdeject", 8) == 0 && cmdargc >= 2) {
                    cdrom_mount(atoi(xargv[1]), "");
                } else if (strncasecmp(xargv[0], "moeject", 8) == 0 && cmdargc >= 2) {
                    mo_eject(atoi(xargv[1]));
                } else if (strncasecmp(xargv[0], "carteject", 8) == 0 && cmdargc >= 2) {
                    cartridge_eject(atoi(xargv[1]));
                } else if (strncasecmp(xargv[0], "zipeject", 8) == 0 && cmdargc >= 2) {
                    zip_eject(atoi(xargv[1]));
                } else if (strncasecmp(xargv[0], "fddload", 7) == 0 && cmdargc >= 4) {
                    uint8_t id, wp;
                    bool    err = false;
                    char    fn[PATH_MAX];
                    memset(fn, 0, sizeof(fn));
                    if (!xargv[2] || !xargv[1]) {
                        free(line);
                        free(linecpy);
                        line = NULL;
                        continue;
                    }
                    err = process_media_commands_3(&id, fn, &wp, cmdargc);
                    if (!err) {
                        if (fn[strlen(fn) - 1] == '\''
                            || fn[strlen(fn) - 1] == '"')
                            fn[strlen(fn) - 1] = '\0';
                        printf("Inserting disk into floppy drive %c: %s\n", id + 'A', fn);
                        floppy_mount(id, fn, wp);
                    }
                } else if (strncasecmp(xargv[0], "moload", 7) == 0 && cmdargc >= 4) {
                    uint8_t id, wp;
                    bool    err = false;
                    char    fn[PATH_MAX];
                    memset(fn, 0, sizeof(fn));
                    if (!xargv[2] || !xargv[1]) {
                        free(line);
                        free(linecpy);
                        line = NULL;
                        continue;
                    }
                    err = process_media_commands_3(&id, fn, &wp, cmdargc);
                    if (!err) {
                        if (fn[strlen(fn) - 1] == '\''
                            || fn[strlen(fn) - 1] == '"')
                            fn[strlen(fn) - 1] = '\0';
                        printf("Inserting into mo drive %hhu: %s\n", id, fn);
                        mo_mount(id, fn, wp);
                    }
                } else if (strncasecmp(xargv[0], "cartload", 7) == 0 && cmdargc >= 4) {
                    uint8_t id, wp;
                    bool    err = false;
                    char    fn[PATH_MAX];
                    memset(fn, 0, sizeof(fn));
                    if (!xargv[2] || !xargv[1]) {
                        free(line);
                        free(linecpy);
                        line = NULL;
                        continue;
                    }
                    err = process_media_commands_3(&id, fn, &wp, cmdargc);
                    if (!err) {
                        if (fn[strlen(fn) - 1] == '\''
                            || fn[strlen(fn) - 1] == '"')
                            fn[strlen(fn) - 1] = '\0';
                        printf("Inserting tape into cartridge holder %hhu: %s\n", id, fn);
                        cartridge_mount(id, fn, wp);
                    }
                } else if (strncasecmp(xargv[0], "zipload", 7) == 0 && cmdargc >= 4) {
                    uint8_t id, wp;
                    bool    err = false;
                    char    fn[PATH_MAX];
                    memset(fn, 0, sizeof(fn));
                    if (!xargv[2] || !xargv[1]) {
                        free(line);
                        free(linecpy);
                        line = NULL;
                        continue;
                    }
                    err = process_media_commands_3(&id, fn, &wp, cmdargc);
                    if (!err) {
                        if (fn[strlen(fn) - 1] == '\''
                            || fn[strlen(fn) - 1] == '"')
                            fn[strlen(fn) - 1] = '\0';
                        printf("Inserting disk into ZIP drive %c: %s\n", id + 'A', fn);
                        zip_mount(id, fn, wp);
                    }
                }
                free(line);
                free(linecpy);
                line = NULL;
            }
        }
    }
#endif
}

int
main(int argc, char **argv)
{
    void     *libedithandle;

    pc_init(argc, argv);
    if (!pc_init_modules()) {
        ui_msgbox_header(MBX_FATAL, L"No ROMs found.", L"86Box could not find any usable ROM images.\n\nPlease download a ROM set and extract it into the \"roms\" directory.");
        return 6;
    }

   // gfxcard[1]   = 0;

    libedithandle = dlopen(LIBEDIT_LIBRARY, RTLD_LOCAL | RTLD_LAZY);
    if (libedithandle) {
        f_readline    = dlsym(libedithandle, "readline");
        f_add_history = dlsym(libedithandle, "add_history");
        if (!f_readline) {
            fprintf(stderr, "Warning: readline in libedit not found, line editing will be limited.\n");
        }
        f_rl_callback_handler_remove = dlsym(libedithandle, "rl_callback_handler_remove");
    } else
        fprintf(stderr, "Warning: libedit not found, line editing will be limited.\n");

    /* Fire up the machine. */
    pc_reset_hard_init();

    /* Initialize the rendering window, or fullscreen. */
    do_start();

    // Unpause the emulated machine.
    plat_pause(0);

    //
    thread_create(monitor_thread, NULL);

    // Start emulation
    main_thread(NULL);

    printf("\n");
    if (f_rl_callback_handler_remove)
        f_rl_callback_handler_remove();
    return 0;
}

