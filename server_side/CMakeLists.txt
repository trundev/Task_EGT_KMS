project(ServerSide)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

add_executable(chat_server
    main.cpp
    client_connection.cpp
    user_data.cpp
    ../common/connection.cpp
    )
