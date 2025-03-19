add_executable(mononoke_test_client
    mononoke_test_client.cpp
)


target_link_libraries(mononoke_test_client PRIVATE 
    Boost::headers # For ASIO
    protobuf::libprotobuf
    mononoke_util
    mononoke_pb
)

target_compile_features(mononoke_test_client PRIVATE
    cxx_std_20 # For Coroutines
)