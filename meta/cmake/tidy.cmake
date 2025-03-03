add_custom_target(
    check_tidy
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/meta/scripts/run-clang-tidy.sh' check '$$DI_TIDY_ARGS'"
    USES_TERMINAL
)

add_custom_target(
    tidy
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/meta/scripts/run-clang-tidy.sh' tidy '$$DI_TIDY_ARGS'"
    USES_TERMINAL
)

add_custom_target(
    clang_analyze
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/meta/scripts/run-clang-tidy.sh' analyze '$$DI_TIDY_ARGS'"
    USES_TERMINAL
)
