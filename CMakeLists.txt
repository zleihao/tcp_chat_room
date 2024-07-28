cmake_minimum_required(VERSION 3.24.2)

project(tcp_chat)

set(CMAKE_C_STANDARD 99)

include_directories("./inc")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "./")


add_executable(chat ./src/server.c)
target_link_libraries(chat pthread)