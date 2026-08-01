function(example target)
	add_executable(${target} ${target}.c)
	target_link_libraries(${target} gamesh srvsh)
	target_include_directories(${target} PUBLIC ../src/gamesh)
endfunction()

configure_file(game.sh game.sh COPYONLY)
file(COPY images DESTINATION .)

example(keyboard-example)
example(surface-example)
example(tick-example)
