cmake_minimum_required(VERSION 3.11)
project(ws_display LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(app)

include(FetchContent)
FetchContent_Declare(i3ipcpp
                     GIT_REPOSITORY https://github.com/drmgc/i3ipcpp.git)

FetchContent_MakeAvailable(i3ipcpp)
