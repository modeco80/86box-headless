# Mononoke utility library
add_library(mononoke_util
        # utility code        
        util/buffer_pool.cpp
        util/highprec_timer.cpp

        # ASIO util
        util/asio_util/mononoke_framed.cpp

        # gfx
        util/gfx/surface.cpp
        util/gfx/difftile.cpp
)

target_compile_features(mononoke_util PUBLIC
    cxx_std_20 # For Coroutines
)