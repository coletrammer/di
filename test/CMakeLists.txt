cmake_minimum_required(VERSION 3.21)

project(diTests LANGUAGES CXX)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
    find_package(di REQUIRED)
    enable_testing()
endif()

# ---- Tests ----

file(GLOB sources CONFIGURE_DEPENDS src/*.cpp)
add_executable(di_test ${sources})

target_link_libraries(di_test PRIVATE di::di)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(di_di INTERFACE -fconstexpr-steps=10000000)
endif()

add_test(NAME di_test COMMAND di_test)
