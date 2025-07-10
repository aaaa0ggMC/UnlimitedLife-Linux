aux_source_directory("./src" UC-SRC)

# much like a server
add_executable(ULCore ${UC-SRC})
set_target_properties(ULCore PROPERTIES LINKER_LANGUAGE CXX)

