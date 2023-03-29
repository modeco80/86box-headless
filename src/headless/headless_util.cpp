//
// Created by lily on 2/10/23.
//

#include <cstring>

extern "C" {

int
stricmp(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
}

int
strnicmp(const char *s1, const char *s2, size_t n)
{
    return strncasecmp(s1, s2, n);
}

// From musl.
char *
local_strsep(char **str, const char *sep)
{
    char *s = *str;
    char *end;

    if (!s)
        return nullptr;

    end = s + strcspn(s, sep);

    if (*end)
        *end++ = 0;
    else
        end = nullptr;

    *str = end;
    return s;
}

}