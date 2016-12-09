cmake_minimum_required(VERSION 2.8)
PROJECT(syncmaster)

SET(SRC_LIST
    main.cpp
    timer.cpp
    event.cpp
    socket.cpp
    sync.cpp
    queue.cpp
    checksum.cpp
    )
ADD_EXECUTABLE(syncmaster ${SRC_LIST})
TARGET_LINK_LIBRARIES(syncmaster pthread)

