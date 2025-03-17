#include <cstdio>

#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <86box/plat.h>

extern "C" {
void *
plat_mmap(size_t size, uint8_t executable)
{
    if (auto ptr = mmap(0, size, PROT_READ | PROT_WRITE | (executable ? PROT_EXEC : 0), MAP_ANON | MAP_PRIVATE, -1, 0); ptr != MAP_FAILED)
        return ptr;

    return nullptr;
}

void
plat_munmap(void *ptr, size_t size)
{
    munmap(ptr, size);
}
}