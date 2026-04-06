cmake_minimum_required(VERSION 3.16)
project(Jackal VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Cross-platform: detect OS
if(ANDROID)
    message(STATUS "Building for Android")
elseif(WIN32)
    message(STATUS "Building for Windows")
elseif(APPLE)
    message(STATUS "Building for macOS/iOS")
else()
    message(STATUS "Building for Linux")
endif()

add_subdirectory(core)
add_subdirectory(qml-client)
