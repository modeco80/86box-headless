//
// Created by lily on 2/10/23.
//


#include <86box/plat_dynld.h>


#include <dlfcn.h>

extern "C" {

void *
dynld_module(const char *name, dllimp_t *table)
{
    dllimp_t *imp = table;

    void     *modhandle = dlopen(name, RTLD_LAZY | RTLD_GLOBAL);
    if (modhandle) {
        for (; imp->name != nullptr; imp++) {
            if ((*(void **) imp->func = dlsym(modhandle, imp->name)) == nullptr) {
                dlclose(modhandle);
                return nullptr;
            }
        }

    }
    return modhandle;
}


void
dynld_close(void *handle)
{
    dlclose(handle);
}

}