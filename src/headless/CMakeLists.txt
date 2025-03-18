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


# Add required things for ramp
find_package(Boost REQUIRED)
find_package(Protobuf REQUIRED)


add_library(plat OBJECT 
        # utility code
        util/buffer_pool.cpp
        util/highprec_timer.cpp

        # Headless platform source files
        platform/headless_mmap.cpp
        platform/headless_path.cpp
        platform/headless_netsocket.cpp
        platform/headless_fs.cpp
        platform/headless_dynld.cpp
        platform/headless_util.cpp
        platform/headless_timer.cpp
        platform/dummy_cdrom_ioctl.c

        # Mononoke server
        mononoke_server.cpp
        mononoke_video.cpp
        mononoke_sound.cpp

        # Main
        headless_main.cpp
        )

# Mononoke Protocol Buffers definitions
add_library(mononoke_pb 
    proto/mononoke_client.proto
    proto/mononoke_server.proto
    proto/mononoke_shared.proto
)

protobuf_generate(
    TARGET mononoke_pb
    LANGUAGE cpp
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(mononoke_pb PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(plat PRIVATE 
    Boost::headers # For ASIO
    protobuf::libprotobuf
    mononoke_pb
)


target_compile_features(plat PRIVATE
    cxx_std_20 # For Coroutines
)


if (NOT CPPTHREADS)
    message(FATAL_ERROR "headless only compiles with CPPTHREADS=ON")
endif()

#set(THREADS_PREFER_PTHREAD_FLAG TRUE)
#find_package(Threads REQUIRED)
#target_link_libraries(86Box Threads::Threads)

add_library(ui OBJECT ui/headless_ui.c ui/headless_cdrom.c)
target_compile_definitions(ui PUBLIC _FILE_OFFSET_BITS=64)
target_link_libraries(ui ${CMAKE_DL_LIBS})
