cmake_minimum_required(VERSION 3.21)
project(MPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" OFF)
option(ENABLE_HARDWARE_DECODER "Enable hardware decoder" ON)

add_subdirectory(third_party/ffmpeg)
add_subdirectory(third_party/wxWidgets)
add_subdirectory(src/core)

if(WIN32)
    add_subdirectory(src/platform/windows)
elseif(ANDROID)
    add_subdirectory(src/platform/android)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
