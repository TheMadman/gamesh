function(example target)
	add_executable(${target} ${target}.c)
	target_link_libraries(${target} gamesh srvsh)
	target_include_directories(${target} PUBLIC ../src/gamesh)
endfunction()

file(COPY game.sh DESTINATION .)

example(event-emitter)
example(event-listener)
example(keyboard-test)
