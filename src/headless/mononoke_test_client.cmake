
find_package(SDL2 REQUIRED)

add_executable(mononoke_test_client
    test_client/mononoke_test_client.cpp
    test_client/sdl_window.cpp
)


target_link_libraries(mononoke_test_client PRIVATE 
    Boost::headers # For ASIO
    protobuf::libprotobuf
    SDL2::SDL2
    mononoke_util
    mononoke_pb
)

target_compile_features(mononoke_test_client PRIVATE
    cxx_std_20 # For Coroutines
)