//
// Created by lily on 2/10/23.
//

#include <cstring>
#include <cstdint>

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

// Serial passthrough device - for now, we explicitly do *not* support this
// and essentially stub it out; later on I might implement it

void 
plat_serpt_write(void *priv, uint8_t data) 
{
    return;
}

int 
plat_serpt_read(void *priv, uint8_t *data) 
{
    *data = 0xff;
    return 1;
}

int 
plat_serpt_open_device(void *priv) 
{
    return 1;
}

void
plat_serpt_close(void *priv) 
{
    return;
}

void 
plat_serpt_set_params(void *priv) 
{
    return;
}

}