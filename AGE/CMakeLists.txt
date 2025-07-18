aux_source_directory("./src" AGE-SRC)
add_library(AGE SHARED ${AGE-SRC})
set_target_properties(AGE PROPERTIES LINKER_LANGUAGE CXX)


# target_compile_options(AGE PUBLIC "-Winline") # 查看内联函数
target_compile_definitions(AGE PRIVATE AGE_BUILD_DLL)
if(WIN32)	
	target_link_libraries(AGE PRIVATE Shcore aaaa0ggmcLib glu32 glfw3.dll glew32.dll opengl32 gdi32 user32 kernel32)
	target_compile_definitions(AGE PRIVATE GLFW_DLL)
else()
	target_link_libraries(AGE PRIVATE GLU OpenGL GLEW glfw aaaa0ggmcLib)
endif()

target_compile_definitions(AGE PUBLIC AGE_BUILD_DLL)

aux_source_directory("./age_test/agetest-src/" AGETEST_SRC)
add_executable(agetest ${AGETEST_SRC})
#target_compile_options(agetest PUBLIC "-O3")
target_compile_definitions(agetest PRIVATE AGE_EM_DEBUG)
if(WIN32)
target_link_libraries(agetest PRIVATE imgui AGE glfw3.dll glew32.dll opengl32 aaaa0ggmcLib)
target_compile_definitions(agetest PRIVATE GLFW_DLL)
else()
# 直接链接
target_link_libraries(agetest PRIVATE imgui AGE aaaa0ggmcLib glfw GLEW OpenGL) 
endif()

