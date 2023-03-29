/*
* 86Box    A hypervisor and IBM PC system emulator that specializes in
*          running old operating systems and software designed for IBM
*          PC systems and compatibles from 1981 through fairly recent
*          system designs based on the PCI bus.
*
*          This file is part of the 86Box distribution.
*
*          File I/O functions for headless platform
*
*
*
* Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
*          Miran Grca, <mgrca8@gmail.com>
*          Fred N. van Kempen, <decwiz@yahoo.com>
*
*          Copyright 2008-2020 Sarah Walker.
*          Copyright 2016-2020 Miran Grca.
*          Copyright 2017-2020 Fred N. van Kempen.
*          Copyright 2021      Laci bá'
*          Copyright 2021      dob205
*          Copyright 2023      modeco80
*/

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cstdint>


#include <sys/time.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include <86box/plat.h>
#include <86box/plat_dir.h>

// problematic headers
extern "C" {
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/path.h>
}

extern "C" {

// defined in headless_util.cpp
char *
local_strsep(char **str, const char *sep);

int
plat_getcwd(char *bufp, int max)
{
    return getcwd(bufp, max) != 0;
}

int
plat_chdir(char *str)
{
    return chdir(str);
}



FILE *
plat_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

FILE *
plat_fopen64(const char *path, const char *mode)
{
    return fopen(path, mode);
}

void
plat_get_exe_name(char *s, int size)
{
    //char *basepath = SDL_GetBasePath();
    //snprintf(s, size, "%s%s", basepath, basepath[strlen(basepath) - 1] == '/' ? "86box" : "/86box");
}

void
plat_init_rom_paths(void)
{
#ifndef __APPLE__
    if (getenv("XDG_DATA_HOME")) {
        char xdg_rom_path[1024] = { 0 };
        strncpy(xdg_rom_path, getenv("XDG_DATA_HOME"), 1024);
        path_slash(xdg_rom_path);
        strncat(xdg_rom_path, "86Box/", 1024);

        if (!plat_dir_check(xdg_rom_path))
            plat_dir_create(xdg_rom_path);
        strcat(xdg_rom_path, "roms/");

        if (!plat_dir_check(xdg_rom_path))
            plat_dir_create(xdg_rom_path);
        rom_add_path(xdg_rom_path);
    } else {
        char home_rom_path[1024] = { 0 };
        snprintf(home_rom_path, 1024, "%s/.local/share/86Box/", getenv("HOME") ? getenv("HOME") : getpwuid(getuid())->pw_dir);

        if (!plat_dir_check(home_rom_path))
            plat_dir_create(home_rom_path);
        strcat(home_rom_path, "roms/");

        if (!plat_dir_check(home_rom_path))
            plat_dir_create(home_rom_path);
        rom_add_path(home_rom_path);
    }
    if (getenv("XDG_DATA_DIRS")) {
        char *xdg_rom_paths      = strdup(getenv("XDG_DATA_DIRS"));
        char *xdg_rom_paths_orig = xdg_rom_paths;
        char *cur_xdg_rom_path   = NULL;
        if (xdg_rom_paths) {
            while (xdg_rom_paths[strlen(xdg_rom_paths) - 1] == ':') {
                xdg_rom_paths[strlen(xdg_rom_paths) - 1] = '\0';
            }
            while ((cur_xdg_rom_path = local_strsep(&xdg_rom_paths, ";")) != NULL) {
                char real_xdg_rom_path[1024] = { '\0' };
                strcat(real_xdg_rom_path, cur_xdg_rom_path);
                path_slash(real_xdg_rom_path);
                strcat(real_xdg_rom_path, "86Box/roms/");
                rom_add_path(real_xdg_rom_path);
            }
        }
        free(xdg_rom_paths_orig);
    } else {
        rom_add_path("/usr/local/share/86Box/roms/");
        rom_add_path("/usr/share/86Box/roms/");
    }
#else
    char  default_rom_path[1024] = { '\0 ' };
    getDefaultROMPath(default_rom_path);
    rom_add_path(default_rom_path);
#endif
}

void
plat_get_global_config_dir(char *strptr)
{
#ifdef __APPLE__
    char* prefPath = SDL_GetPrefPath(NULL, "net.86Box.86Box")
#else
    //char* prefPath = SDL_GetPrefPath(NULL, "86Box");
#endif
    //strncpy(strptr, prefPath, 1024);
    // path_slash(strptr);
}

void
plat_put_backslash(char *s)
{
    int c = strlen(s) - 1;

    if (s[c] != '/')
        s[c] = '/';
}

/* Return the last element of a pathname. */
char *
plat_get_basename(const char *path)
{
    int c = (int) strlen(path);

    while (c > 0) {
        if (path[c] == '/')
            return ((char *) &path[c + 1]);
        c--;
    }

    return ((char *) path);
}

int
plat_dir_check(char *path)
{
    struct stat dummy;
    if (stat(path, &dummy) < 0) {
        return 0;
    }
    return S_ISDIR(dummy.st_mode);
}

void
plat_tempfile(char *bufp, char *prefix, char *suffix)
{
    tm     *calendartime;
    timeval t;
    time_t         curtime;

    if (prefix != NULL)
        sprintf(bufp, "%s-", prefix);
    else
        strcpy(bufp, "");
    gettimeofday(&t, NULL);
    curtime      = time(NULL);
    calendartime = localtime(&curtime);
    sprintf(&bufp[strlen(bufp)], "%d%02d%02d-%02d%02d%02d-%03ld%s", calendartime->tm_year, calendartime->tm_mon, calendartime->tm_mday, calendartime->tm_hour, calendartime->tm_min, calendartime->tm_sec, t.tv_usec / 1000, suffix);
}

int
plat_dir_create(char *path)
{
    return mkdir(path, S_IRWXU);
}

void
plat_remove(char *path)
{
    remove(path);
}


}