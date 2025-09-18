cmake_minimum_required(VERSION 3.10)

project(PublicChat)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()

add_subdirectory(client_side)
add_subdirectory(server_side)
