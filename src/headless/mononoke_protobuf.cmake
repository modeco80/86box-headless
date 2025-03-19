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
