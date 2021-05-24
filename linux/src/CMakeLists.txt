file(GLOB_RECURSE SRC_LIST *.cpp *.h *.hpp)
add_executable(${PROJECT_NAME} ${SRC_LIST})

target_include_directories(${PROJECT_NAME} PRIVATE ${cli11_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} i3ipc++)
target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} serial)

target_compile_options(
    ${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wconversion
    -Wsign-conversion -Wshadow)

install(TARGETS ${PROJECT_NAME})
