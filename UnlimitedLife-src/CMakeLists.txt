aux_source_directory("./" UL_SRC)
add_executable(UnlimitedLife ${UL_SRC})
set_target_properties(UnlimitedLife PROPERTIES LINKER_LANGUAGE CXX)
