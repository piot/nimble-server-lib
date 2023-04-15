cmake_minimum_required(VERSION 3.17)
project(nimble_server C)

if (DEFINED PLATFORM)
  if (PLATFORM STREQUAL "OS64" )
      add_compile_definitions(TORNADO_OS_IOS)
      set(OS_IOS 1)
      set(OS_NAME "ios")
  endif()
elseif (APPLE)
  add_compile_definitions(TORNADO_OS_MACOS)
  set(OS_MACOS 1)
  set(OS_NAME "macos")
elseif (UNIX)
  add_compile_definitions(TORNADO_OS_LINUX)
  set(OS_LINUX 1)
  set(OS_NAME "linux")
elseif (WIN32)
  add_compile_definitions(TORNADO_OS_WINDOWS)
  set(OS_WINDOWS 1)
  set(OS_NAME "windows")
endif()

set(deps ../../deps/)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(isDebug TRUE)
else()
  message("no debug")
  set(isDebug FALSE)
endif()

file(GLOB_RECURSE deps_src FOLLOW_SYMLINKS
  "${deps}*/src/lib/*.c"
)

file(GLOB_RECURSE lib_src FOLLOW_SYMLINKS
  "*.c"
)

add_library(nimble_server_lib
  ${deps_src}
  ${lib_src}
)

set_property(TARGET nimble_server_lib PROPERTY C_STANDARD 11)

set_target_properties(nimble_server_lib
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if (OS_WINDOWS)
  target_compile_options(nimble_server_lib PRIVATE -Wall -pedantic -W4)
else()
  target_compile_options(nimble_server_lib PRIVATE -Wall -Wextra -Wshadow -Wstrict-aliasing -ansi -pedantic -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)
endif()

if (isDebug)
  message("Debug build detected")
  add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)

  set(CONFIGURATION_DEBUG 1)
else()
  message("Release build detected")
  target_compile_options(nimble_server_lib PRIVATE -ffast-math -O3)
  target_link_options(nimble_server_lib PRIVATE -s)
endif()

# target_link_options(nimble_server_lib PRIVATE -static-libgcc)

target_include_directories(nimble_server_lib PRIVATE
  ${deps}piot/tiny-libc/src/include # needed from flood-c
  ${deps}piot/flood-c/src/include
  ${deps}piot/monotonic-time-c/src/include
  ${deps}piot/stats-c/src/include
  ${deps}piot/udp-server-c/src/include
  ${deps}piot/nimble-steps-c/src/include
  ${deps}piot/nimble-steps-serialize-c/src/include
  ${deps}piot/nimble-serialize-c/src/include
  ${deps}piot/discoid-c/src/include
  ${deps}piot/clog/src/include
  ${deps}piot/imprint/src/include
  ${deps}piot/blob-stream/src/include
  ${deps}piot/bit-array/src/include
  ${deps}piot/udp-transport/src/include
        ../include
)

if (OS_WINDOWS)
  target_link_libraries(nimble_server_lib ws2_32)
else()
  target_link_libraries(nimble_server_lib m)
endif()
