aux_source_directory("./src" MF-SRC)
add_library(MineForge SHARED ${MF-SRC})
set_target_properties(MineForge PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(MineForge PUBLIC MF_BUILD_DLL)

