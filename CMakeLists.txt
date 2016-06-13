cmake_minimum_required(VERSION 2.8)
project(waveOverUDP)

message("Build type: ${CMAKE_BUILD_TYPE}")

get_filename_component(BUILD_DIR_NAME ${CMAKE_BINARY_DIR} NAME)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../${BUILD_DIR_NAME}-bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../${BUILD_DIR_NAME}-bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../${BUILD_DIR_NAME}-bin")

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -Wall -fvisibility=hidden")
else()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO /INCREMENTAL:NO")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MANIFEST:NO /INCREMENTAL:NO")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO /INCREMENTAL:NO")
endif()

if(WIN32)
  set(TARGET_OS "win")
  set(TARGET_LIBS ws2_32 winmm avrt)
else()
  set(TARGET_OS "unix")
  set(TARGET_LIBS asound)
endif()

add_executable(wavePlayUDP #WIN32
  "${CMAKE_CURRENT_LIST_DIR}/${TARGET_OS}/wavePlayUDP.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Logger.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Logger.h")
target_link_libraries(wavePlayUDP ${TARGET_LIBS})

add_executable(waveSendUDP
  "${CMAKE_CURRENT_LIST_DIR}/${TARGET_OS}/waveSendUDP.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Logger.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Logger.h")
target_link_libraries(waveSendUDP ${TARGET_LIBS})
