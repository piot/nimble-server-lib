cmake_minimum_required(VERSION 3.17)
add_subdirectory(lib)

if(NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()
