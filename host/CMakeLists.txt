cmake_minimum_required(VERSION 3.10)
project(GestureHost)

set(CMAKE_CXX_STANDARD 14)

add_executable(gesture_host
    src/main.cpp
    src/com_port.cpp
    src/packet_parser.cpp
)

target_link_libraries(gesture_host ole32)