aux_source_directory("./src" ALIB4-SRC)
add_library(aaaa0ggmcLib-g4 SHARED ${ALIB4-SRC})
set_target_properties(aaaa0ggmcLib-g4 PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(aaaa0ggmcLib-g4 PUBLIC ALIB4_BUILD_DLL)

aux_source_directory("./alib4_test/test-src" TEST-SRC)
add_executable(alib4test ${TEST-SRC})
target_link_libraries(alib4test PRIVATE AGE aaaa0ggmcLib glfw GLEW OpenGL)
