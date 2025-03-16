aux_source_directory("./src" AGE-SRC)
add_library(AGE SHARED ${AGE-SRC})
set_target_properties(AGE PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(AGE PRIVATE OpenGL GLEW glfw)
target_link_options(AGE PUBLIC "-Wl,--allow-multiple-definition")
target_compile_definitions(AGE PUBLIC AGE_BUILD_DLL)

aux_source_directory("./age_test/agetest-src/" AGETEST_SRC)
add_executable(agetest ${AGETEST_SRC})
target_link_libraries(agetest PRIVATE AGE aaaa0ggmcLib glfw)
target_link_options(agetest PUBLIC "-Wl,--rpath,./")

