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
double mouse_sensitivity;

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
plat_vidapi(char *api)
{
    // Always initalize VNC
    vnc_init(NULL);

    endblit();
    device_force_redraw();

    return 1;
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
plat_resize(int w, int h)
{
    vnc_resize(w, h);
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

