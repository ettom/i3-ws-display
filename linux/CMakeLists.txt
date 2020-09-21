cmake_minimum_required(VERSION 3.11)
project(ws-display LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -ggdb \
    -fno-limit-debug-info -fsanitize=undefined \
    -fsanitize=implicit-conversion -fsanitize=integer \
    -fno-limit-debug-info")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
endif()

add_subdirectory(src)

include(FetchContent)
FetchContent_Declare(i3ipcpp
    GIT_REPOSITORY https://github.com/drmgc/i3ipcpp.git)

FetchContent_MakeAvailable(i3ipcpp)
