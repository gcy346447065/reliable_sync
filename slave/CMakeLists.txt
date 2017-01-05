cmake_minimum_required(VERSION 2.8)
PROJECT(syncslave)

SET(SRC_LIST
    main.cpp
    timer.cpp
    event.cpp
    socket.cpp
    sync.cpp
    checksum.cpp
    send.cpp
    recv.cpp
    )
ADD_EXECUTABLE(syncslave ${SRC_LIST})
TARGET_LINK_LIBRARIES(syncslave pthread)

