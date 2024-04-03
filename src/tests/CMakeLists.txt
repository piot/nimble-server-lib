cmake_minimum_required(VERSION 3.17)
project(nimble-server-lib C)
enable_testing()

set(CMAKE_C_STANDARD 99)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()

if(isDebug)
    message("Debug build detected")
    add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)
    set(CONFIGURATION_DEBUG 1)
endif()

add_executable(nimble_server_tests main.c test.c)

add_test(NAME nimble_server_tests COMMAND nimble_server_tests)

if(WIN32)
    target_link_libraries(nimble_server_tests nimble-server-lib)
else()
    target_link_libraries(nimble_server_tests nimble-server-lib m)
endif(WIN32)
