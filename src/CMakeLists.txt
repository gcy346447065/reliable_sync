cmake_minimum_required(VERSION 2.8)
PROJECT(reliableSync)

SET(SRC_LIST
    main.cpp
    event.cpp
    list_instant.cpp
    list_waited.cpp
    mbufer.cpp
    #sync_master.cpp
    #sync_slave.cpp
    timer.cpp
    vos.cpp
    )

ADD_EXECUTABLE(reliableSync ${SRC_LIST})
TARGET_LINK_LIBRARIES(reliableSync pthread)
