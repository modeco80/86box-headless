#
# 86Box    A hypervisor and IBM PC system emulator that specializes in
#          running old operating systems and software designed for IBM
#          PC systems and compatibles from 1981 through fairly recent
#          system designs based on the PCI bus.
#
#          This file is part of the 86Box distribution.
#
#          CMake build script.
#
# Authors: Cacodemon345
#          David Hrdlička, <hrdlickadavid@outlook.com>
#
#          Copyright 2021      Cacodemon345.
#          Copyright 2021      David Hrdlička.
#
add_library(plat OBJECT headless.c
        headless_path.cpp
        headless_fs.cpp
        headless_dynld.cpp
        headless_util.cpp
        headless_timer.cpp)

if (NOT CPPTHREADS)
    message(FATAL_ERROR "headless only compiles with CPPTHREAD=ON")
endif()

#set(THREADS_PREFER_PTHREAD_FLAG TRUE)
#find_package(Threads REQUIRED)
#target_link_libraries(86Box Threads::Threads)

add_library(ui OBJECT headless_ui.c headless_cdrom.c)
target_compile_definitions(ui PUBLIC _FILE_OFFSET_BITS=64)
target_link_libraries(ui ${CMAKE_DL_LIBS})
