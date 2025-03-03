if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_INSTALL_INCLUDEDIR
        "include/di-${PROJECT_VERSION}"
        CACHE STRING ""
    )
    set_property(CACHE CMAKE_INSTALL_INCLUDEDIR PROPERTY TYPE PATH)
endif()

# Project is configured with no languages, so tell GNUInstallDirs the lib dir
set(CMAKE_INSTALL_LIBDIR
    lib
    CACHE PATH ""
)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package di)

install(
    TARGETS di_di
    EXPORT diTargets
    INCLUDES
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILE_SET HEADERS
)

write_basic_package_version_file("${package}ConfigVersion.cmake" COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
# Allow package maintainers to freely override the path for the configs
set(di_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_DATADIR}/${package}"
    CACHE STRING "CMake package config location relative to the install prefix"
)
set_property(CACHE di_INSTALL_CMAKEDIR PROPERTY TYPE PATH)
mark_as_advanced(di_INSTALL_CMAKEDIR)

install(
    FILES meta/cmake/install-config.cmake
    DESTINATION "${di_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT di_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${di_INSTALL_CMAKEDIR}"
    COMPONENT di_Development
)

install(
    EXPORT diTargets
    NAMESPACE di::
    DESTINATION "${di_INSTALL_CMAKEDIR}"
    COMPONENT di_Development
)

if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
endif()
