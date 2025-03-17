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

# Flatbuffers
set(FLATBUFFERS_INSTALL OFF)
set(FLATBUFFERS_BUILD_TESTS OFF)
add_subdirectory(flatbuffers)

add_library(plat OBJECT 
        headless.c
        headless_path.cpp
        headless_netsocket.cpp
        headless_fs.cpp
        headless_dynld.cpp
        headless_util.cpp
        headless_timer.cpp

        dummy_cdrom_ioctl.c
        
        # RAMP
        rampserver.cpp
        rampvideo.cpp
        rampsound.cpp
        )


flatbuffers_generate_headers(
	TARGET ramp_proto_generated
	SCHEMAS ${CMAKE_CURRENT_LIST_DIR}/proto/ramp_client.fbs ${CMAKE_CURRENT_LIST_DIR}/proto/ramp_server.fbs
	FLAGS --cpp-std c++17 --cpp-field-case-style lower --scoped-enums --filename-ext hpp
)

target_link_libraries(plat PRIVATE 
    Boost::headers # For ASIO
    FlatBuffers::FlatBuffers
    ramp_proto_generated
)

target_include_directories(plat PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
)


if (NOT CPPTHREADS)
    message(FATAL_ERROR "headless only compiles with CPPTHREADS=ON")
endif()

#set(THREADS_PREFER_PTHREAD_FLAG TRUE)
#find_package(Threads REQUIRED)
#target_link_libraries(86Box Threads::Threads)

add_library(ui OBJECT headless_ui.c headless_cdrom.c)
target_compile_definitions(ui PUBLIC _FILE_OFFSET_BITS=64)
target_link_libraries(ui ${CMAKE_DL_LIBS})
