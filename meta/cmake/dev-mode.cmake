include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

option(ENABLE_COVERAGE "Enable coverage support separate from CTest's" OFF)
if(ENABLE_COVERAGE)
    include(meta/cmake/coverage.cmake)
endif()

include(meta/cmake/tidy.cmake)
