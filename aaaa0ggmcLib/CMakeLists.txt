aux_source_directory("./src" ALIB-SRC)

add_library(aaaa0ggmcLib SHARED ${ALIB-SRC})
set_target_properties(aaaa0ggmcLib PROPERTIES LINKER_LANGUAGE CXX)
target_link_options(aaaa0ggmcLib PRIVATE -fPIC)
target_link_directories(aaaa0ggmcLib PRIVATE rapidjson)
target_link_libraries(aaaa0ggmcLib PUBLIC stdc++exp)
target_compile_definitions(aaaa0ggmcLib PUBLIC BUILD_DLL)

# static one
add_library(aaaa0ggmcLib_static STATIC ${ALIB-SRC})
set_target_properties(aaaa0ggmcLib_static PROPERTIES LINKER_LANGUAGE CXX)
target_link_options(aaaa0ggmcLib_static PRIVATE -fPIC)
target_link_directories(aaaa0ggmcLib_static PRIVATE rapidjson)
target_link_libraries(aaaa0ggmcLib_static PUBLIC stdc++exp)
target_compile_definitions(aaaa0ggmcLib_static PUBLIC BUILD_DLL)

aux_source_directory("./alib_test/simp-biliplayer-src" SIMP_SRC)
add_executable(simp-biliplayer ${SIMP_SRC})
target_link_libraries(simp-biliplayer PRIVATE aaaa0ggmcLib)

aux_source_directory("./alib_test/alibtest0-src" TEST0_SRC)
add_executable(alibtest0 ${TEST0_SRC})
target_link_libraries(alibtest0 PRIVATE aaaa0ggmcLib)

include(GNUInstallDirs)
install(TARGETS aaaa0ggmcLib aaaa0ggmcLib_static
    EXPORT aaaa0ggmcLibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ../CDep/headers/alib-g3
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
install(EXPORT aaaa0ggmcLibTargets
    FILE aaaa0ggmcLibTargets.cmake
    NAMESPACE alib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aaaa0ggmcLib
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfigVersion.cmake"
    VERSION 0.1.0         
    COMPATIBILITY SameMajorVersion
)

configure_file(cmake/aaaa0ggmcLibConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/aaaa0ggmcLibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aaaa0ggmcLib
)
