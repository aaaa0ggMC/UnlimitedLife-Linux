aux_source_directory("./src" ALIB-SRC)
add_library(aaaa0ggmcLib SHARED ${ALIB-SRC})
set_target_properties(aaaa0ggmcLib PROPERTIES LINKER_LANGUAGE CXX)
target_link_directories(aaaa0ggmcLib PRIVATE rapidjson)
target_compile_definitions(aaaa0ggmcLib PUBLIC BUILD_DLL)

aux_source_directory("./alib_test/simp-biliplayer-src" SIMP_SRC)
add_executable(simp-biliplayer ${SIMP_SRC})
target_link_libraries(simp-biliplayer PRIVATE aaaa0ggmcLib)

aux_source_directory("./alib_test/alibtest0-src" TEST0_SRC)
add_executable(alibtest0 ${TEST0_SRC})
target_link_libraries(alibtest0 PRIVATE aaaa0ggmcLib)


