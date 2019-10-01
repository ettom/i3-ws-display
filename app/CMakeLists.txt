file(GLOB_RECURSE SRC_LIST *.cpp *.h *.hpp)
add_executable(${PROJECT_NAME} ${SRC_LIST})

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)
target_link_libraries(${PROJECT_NAME} ${JSONCPP_LIBRARIES})

target_include_directories(${PROJECT_NAME}
	PRIVATE
	${JSONCPP_INCLUDE_DIRS}
	)
link_directories(${PROJECT_NAME} ${JSONCPP_LIBRARIES})



target_link_libraries(${PROJECT_NAME} i3ipc++)
target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} serial)
target_link_libraries(${PROJECT_NAME} jsoncpp)


target_compile_options(${PROJECT_NAME}
	PRIVATE
	-Wall -Wextra -Wpedantic -Wconversion
	-Wsign-conversion -Wshadow)

install(TARGETS ${PROJECT_NAME})
