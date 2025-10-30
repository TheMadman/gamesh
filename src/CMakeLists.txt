find_package(SDL3 REQUIRED)

add_library(gamesh SHARED gamesh.c)
add_executable(game-runner game-runner.c fd-manager.c)

target_include_directories(gamesh
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>)
target_include_directories(game-runner
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>)

target_link_libraries(gamesh srvsh)
target_link_libraries(game-runner SDL3::SDL3 srvsh)

install(TARGETS gamesh
	DESTINATION lib)
install(TARGETS game-runner
	DESTINATION bin)
install(FILES gamesh/gamesh.h
	DESTINATION include)
