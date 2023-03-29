//
// Created by lily on 2/10/23.
//

#include <cstring>

extern "C" {

void
path_get_dirname(char *dest, const char *path)
{
    int   c = (int) strlen(path);
    char *ptr;

    ptr = (char *) path;

    while (c > 0) {
        if (path[c] == '/' || path[c] == '\\') {
            ptr = (char *) &path[c];
            break;
        }
        c--;
    }

    /* Copy to destination. */
    while (path < ptr)
        *dest++ = *path++;
    *dest = '\0';
}

int
path_abs(char *path)
{
    return path[0] == '/';
}

void
path_normalize(char *path)
{
    /* No-op. */
}

void
path_slash(char *path)
{
    if ((path[strlen(path) - 1] != '/')) {
        strcat(path, "/");
    }
    path_normalize(path);
}

char *
path_get_filename(char *s)
{
    int c = strlen(s) - 1;

    while (c > 0) {
        if (s[c] == '/' || s[c] == '\\')
            return (&s[c + 1]);
        c--;
    }

    return (s);
}

char *
path_get_extension(char *s)
{
    int c = strlen(s) - 1;

    if (c <= 0)
        return (s);

    while (c && s[c] != '.')
        c--;

    if (!c)
        return (&s[strlen(s)]);

    return (&s[c + 1]);
}

void
path_append_filename(char *dest, const char *s1, const char *s2)
{
    strcpy(dest, s1);
    path_slash(dest);
    strcat(dest, s2);
}


}