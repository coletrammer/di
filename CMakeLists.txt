cmake_minimum_required(VERSION 3.21)

include(meta/cmake/prelude.cmake)

project(
    di
    VERSION 0.0.1
    DESCRIPTION "A free-standing alternative C++ standard library"
    HOMEPAGE_URL "https://coletrammer.github.io/di"
    LANGUAGES NONE
)

include(meta/cmake/variables.cmake)

# ---- Declare library ----

add_library(di_di INTERFACE)
add_library(di::di ALIAS di_di)

set_property(TARGET di_di PROPERTY EXPORT_NAME di)

target_include_directories(di_di ${warning_guard} INTERFACE "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

file(GLOB_RECURSE headers CONFIGURE_DEPENDS include/*.h)
target_sources(
    di_di
    INTERFACE FILE_SET
              HEADERS
              TYPE
              HEADERS
              BASE_DIRS
              include
              FILES
              ${headers}
)

target_compile_options(di_di INTERFACE -std=gnu++26)
target_compile_options(di_di INTERFACE -fno-exceptions -fno-rtti)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(di_di INTERFACE -fsized-deallocation)
endif()

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(meta/cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT di_DEVELOPER_MODE)
    return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
    message(AUTHOR_WARNING "Developer mode is intended for developers of di")
endif()

include(meta/cmake/dev-mode.cmake)
