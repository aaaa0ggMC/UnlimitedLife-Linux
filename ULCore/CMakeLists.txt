aux_source_directory("./src" UC-SRC)

# add files orderly as a set
set(MR "../CDep/headers/ul")
set(UC-MODULES
    "${MR}/base/requests.ixx"
    "${MR}/terrain/block.ixx"
    "${MR}/terrain/chunk.ixx"
    "${MR}/terrain/base.ixx"
    "${MR}/ULServer.ixx"
    "${MR}/ULSimpleRenderer.ixx"
)

# much like a server
add_executable(ULCore ${UC-MODULES}  ${UC-SRC})
set_target_properties(ULCore PROPERTIES LINKER_LANGUAGE CXX)
# enable fmodules
target_compile_options(ULCore PRIVATE -fmodules-ts)
# enable c++23 
target_compile_features(ULCore PRIVATE cxx_std_23)

