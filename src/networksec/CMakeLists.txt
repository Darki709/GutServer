cmake_minimum_required(VERSION 3.10)
project(gutweb-networksec LANGUAGES CXX)

set(SOURCES
    baseSocket.cpp
    tcpSocket.cpp
    udpSocket.cpp
)

add_library(networksec STATIC ${SOURCES})

target_include_directories(networksec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(networksec PUBLIC cxx_std_17)

if (WIN32)
    # Winsock is used by the network code on Windows
    target_compile_definitions(networksec PUBLIC -DWIN32)
    target_link_libraries(networksec PUBLIC ws2_32)
endif()
