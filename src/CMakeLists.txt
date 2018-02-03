cmake_minimum_required(VERSION 2.8)
PROJECT(reliableSync)

SET(CMAKE_BUILD_TYPE "Debug")  
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")  
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall") 

SET(SRC_LIST
    main.cpp
    master.cpp
    master_send.cpp
    master_recv.cpp
    slave.cpp
    slave_send.cpp
    slave_recv.cpp
    event.cpp
    mbufer.cpp
    timer.cpp
    checksum.cpp
    vos.cpp
    )

ADD_EXECUTABLE(reliableSync ${SRC_LIST})
TARGET_LINK_LIBRARIES(reliableSync pthread)

