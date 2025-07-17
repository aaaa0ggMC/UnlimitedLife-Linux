cmake_minimum_required(VERSION 3.10)
project(ImGUIGL)

set(CMAKE_CXX_STANDARD 20)

if (WIN32)
    add_compile_options(-ftls-model=global-dynamic)
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--allow-multiple-definition -Wl,--rpath,./") 
else()
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--allow-multiple-definition -Wl,--rpath,./")
endif()

aux_source_directory("./" BASE)

add_library(imgui SHARED ./imgui_impl_opengl3.cpp ./imgui_impl_glfw.cpp ${BASE})
target_compile_definitions(imgui PRIVATE BUILD_DLL)
if(WIN32)	
	target_link_libraries(imgui PRIVATE glu32 glfw3.dll opengl32 gdi32 user32 kernel32)
	target_compile_definitions(imgui PRIVATE GLFW_DLL)
else()
	target_link_libraries(imgui PRIVATE GLU OpenGL glfw)
endif()
