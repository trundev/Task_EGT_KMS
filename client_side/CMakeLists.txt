project(ClientSide)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

# Generate .pb.cc and .pb.h
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_executable(chat_client
    main.cpp
    ../common/connection.cpp
    ${PROTO_SRCS} ${PROTO_HDRS}
    )
target_include_directories(chat_client PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(chat_client ${Protobuf_LIBRARIES})
