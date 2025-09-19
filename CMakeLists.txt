cmake_minimum_required(VERSION 3.10)

project(PublicChat)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/common/messages.proto)

add_subdirectory(client_side)
add_subdirectory(server_side)
