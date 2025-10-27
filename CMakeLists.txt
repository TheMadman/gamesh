cmake_minimum_required(VERSION 3.31.6)

project(GameSH)

add_subdirectory(src)

if (BUILD_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()
