#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <86box/86box.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/video.h>
#include <86box/ui.h>
#include <86box/version.h>

#include <86box/vnc.h>

// Config things we need to define cause the code is messy

int fixed_size_x = 640;
int fixed_size_y = 480;

int mouse_capture = 0;
int kbd_req_capture;

int update_icons;

int hide_tool_bar;
int hide_status_bar;
extern double mouse_sensitivity;

// New UI stuff for 4.0+ ?
int status_icons_fullscreen;

// Needed because 86box code is 86box quality.
#include <86box/qt-glsl.h>
char gl3_shader_file[MAX_USER_SHADERS][512];

void mononoke_video_init(); // mononoke_video.cpp


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

void
ui_sb_update_icon_state(int tag, int state)
{
}

void
ui_sb_update_icon(int tag, int active)
{
}

void
ui_sb_update_tip(int arg)
{
}

void
ui_sb_update_panes(void)
{
}

void
ui_sb_update_text(void)
{
}

void
ui_sb_set_text_w(wchar_t *wstr)
{
}

void 
ui_hard_reset_completed(void) 
{
    /* Stub */
}

int
ui_msgbox(int flags, void *message)
{
    return ui_msgbox_header(flags, NULL, message);
}

int
ui_msgbox_header(int flags, void *header, void *message)
{
    if(header != NULL)
        printf("86Box UI: [%ls] %ls\n", (wchar_t*)header, (wchar_t*)message);
    else
        printf("86Box UI: %ls\n", (wchar_t*)message);
    return 0;
}

void
ui_sb_bugui(char *str)
{
}

void
ui_sb_set_ready(int ready)
{
}

/* API */
void
ui_sb_mt32lcd(char *str)
{
}

int
plat_vidapi(const char *api)
{
    // Initialize Mononoke video driver
    mononoke_video_init();


    endblit();
    device_force_redraw();

    return 1;
}

char *
plat_vidapi_name(int i)
{
    return "mononoke";
}


int
plat_setvid(int api)
{
    return 1;
}

void
plat_mouse_capture(int on)
{
    mouse_capture = on;
}

void
plat_resize(int w, int h, int monitor_index)
{
    //vnc_resize(w, h);
}

wchar_t *
ui_window_title(wchar_t *str)
{
    return L"86Box Headless";
}

void
ui_init_monitor(int monitor_index)
{
}

void
ui_deinit_monitor(int monitor_index)
{
}

void
plat_resize_request(int w, int h, int monitor_index)
{
   atomic_store((&doresize_monitors[monitor_index]), 1);
}

