add_executable(oi.test_stream
    src/test_stream.cpp
)

target_link_libraries(oi.test_stream
    oi.core oi.network oi.rgbd)
