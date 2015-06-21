cmake_minimum_required(VERSION 2.8)
project(waveOverUDP)

set(CMAKE_BUILD_TYPE "Release")

get_filename_component(BUILD_DIR_NAME ${CMAKE_BINARY_DIR} NAME)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/../${BUILD_DIR_NAME}-bin")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/../${BUILD_DIR_NAME}-bin")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -Wall -fvisibility=hidden")

add_executable(wavePlayUDP wavePlayUDP.cpp Logger.cpp Logger.h)
target_link_libraries(wavePlayUDP asound)

add_executable(waveSendUDP waveSendUDP.cpp Logger.cpp Logger.h)
target_link_libraries(waveSendUDP asound)
