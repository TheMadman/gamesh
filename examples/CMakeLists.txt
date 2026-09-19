function(example target)
	add_executable(${target} ${target}.c)
	target_link_libraries(${target} gamesh srvsh)
	target_include_directories(${target} PUBLIC ../src/gamesh)
endfunction()

configure_file(happy.sh happy.sh COPYONLY)
file(COPY images DESTINATION .)

example(keyboard-example)
example(graphic-example)
example(surface-example)
example(animation-example)
example(tick-example)
