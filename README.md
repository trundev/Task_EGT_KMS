Public chat system
===

Console application implementation, with [Protocol Buffers](https://protobuf.dev/) backend. Server is using multi-threaded techniques.
> Uses C++20, at some places could be a bit overkill :)

See also the Python GUI [tkinter](https://docs.python.org/3/library/tkinter.html) implementation in [client_side_tkinter](./client_side_tkinter/).

# Details

## Project Source Tree
```
Task_EGT_KMS/
├── README.md
├── CMakeLists.txt
├── server_side/
│   ├── CMakeLists.txt
│   └── ... server sources/headers
├── client_side/
│   ├── CMakeLists.txt
│   └── ... client sources/headers
├── client_side_tkinter/
│   └── main.py
└── common/
│   ├── CMakeLists.txt
    └── ... common sources/headers
```

## Building

- Install dependencies
  ```
  sudo apt install -y protobuf-compiler
  ```

- Configure step
  ```
  cmake -B build
  ```

- Build step
  ```
  cmake --build build
  ```

- Output binaries
  ```
  ./build/server_side/chat_server
  ./build/client_side/chat_client
  ```

- Python protobuf module for `client_side_tkinter`
  ```
  mkdir -p ./client_side_tkinter/protobuf
  protoc --proto_path=./common --python_out=./client_side_tkinter/protobuf messages.proto
  ```

# Use

- In the terminal for the server:
  ```
  cd build/server_side
  ./chat_server
  ```

  This launches a server that listens for connections on port `8080` (port number is from [defines.h](common/defines.h)).

- In a terminal for cient:

  _Open multiple terminals, replace `<USERNAME>` with the specific name_

  ```
  cd ./build/client_side
  ./chat_client localhost <USERNAME>
  ```

  This connects to the server, logs in as `<USERNAME>`, and prints the welcome message.
  Type a chat message, then press `<enter>` to send. To send a server command, use `!` prefix, like: `!help`, `!list`, `!kickout user`.

- Python tkinter client
  ```
  python3 ./chat_client_tkinter/main.py localhost <USERNAME>
  ```

  > Requires Python 3.13, as `Queue.is_shutdown` property is used to check for closed connection scenario

# To-Do list

- [x] It must consist of two parts – server side and a client side

- [x] A server should be able to handle at least 10 simultaneously connected clients

- [x] The server should be implemented using the C++ programming language

- [x] Messages, sent from one client, should be visible to all other clients

- [x] Each client must identify itself to the server with username

    - [ ] _Allow username change after initial connect_

- [x] All conversations must be written to log file. New file must be created every hour (at 10:00, 11:00, etc.)

- [x] Server must be able to print the connected clients and some basic info (username, ipaddress, time online) upon request

- [x] Server must be able to kick a client out

- [x] Client must be disconnected after 10 minutes inactivity

- [x] Use more complex communication protocol (ex.: protobuf)

- [ ] Create a database to store users and their messages
