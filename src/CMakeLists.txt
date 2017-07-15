cmake_minimum_required(VERSION 2.8)
PROJECT(reliableSync)

SET(SRC_LIST
    main.cpp
    master.cpp
    master_send.cpp
    master_recv.cpp
    slave.cpp
    slave_send.cpp
    slave_recv.cpp
    event.cpp
    list.cpp
    mbufer.cpp
    timer.cpp
    checksum.cpp
    vos.cpp
    )

ADD_EXECUTABLE(reliableSync ${SRC_LIST})
TARGET_LINK_LIBRARIES(reliableSync pthread)
