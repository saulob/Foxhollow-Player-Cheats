cmake_minimum_required(VERSION 3.25)
project(player_cheats VERSION 1.1.0 LANGUAGES C)

include(CheckCSourceCompiles)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Player Cheats supports 64-bit targets only")
endif()

if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
  message(FATAL_ERROR "Build one macOS architecture at a time (CMAKE_OSX_ARCHITECTURES=arm64 or x86_64): "
                      "Foxhollow loads lib/macos-arm64 and lib/macos-x86_64 separately")
endif()

# Ask the compiler which CPU it targets; CMAKE_SYSTEM_PROCESSOR reports the host,
# which is wrong for CMAKE_OSX_ARCHITECTURES builds. The results are not cached, so
# changing the architecture of an existing build directory is detected.
unset(PLAYER_CHEATS_TARGET_X86_64 CACHE)
unset(PLAYER_CHEATS_TARGET_ARM64 CACHE)
check_c_source_compiles("
#if !defined(__x86_64__) && !defined(_M_X64)
#error not x86_64
#endif
int main(void) { return 0; }" PLAYER_CHEATS_TARGET_X86_64)
check_c_source_compiles("
#if !defined(__aarch64__) && !defined(_M_ARM64)
#error not arm64
#endif
int main(void) { return 0; }" PLAYER_CHEATS_TARGET_ARM64)

# Directory names match platform_directory() in Foxhollow's port/src/foxhollow_mods.cpp.
if(WIN32 AND PLAYER_CHEATS_TARGET_X86_64)
  set(MOD_PLATFORM_DIR "windows-amd64")
elseif(APPLE AND PLAYER_CHEATS_TARGET_ARM64)
  set(MOD_PLATFORM_DIR "macos-arm64")
elseif(APPLE AND PLAYER_CHEATS_TARGET_X86_64)
  set(MOD_PLATFORM_DIR "macos-x86_64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND PLAYER_CHEATS_TARGET_ARM64)
  set(MOD_PLATFORM_DIR "linux-arm64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND PLAYER_CHEATS_TARGET_X86_64)
  set(MOD_PLATFORM_DIR "linux-amd64")
else()
  message(FATAL_ERROR "Player Cheats supports Windows x64, Linux x86_64/arm64 and macOS x86_64/arm64; "
                      "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} is not supported yet")
endif()

add_library(player_cheats SHARED
  src/mod.c
  src/player_cheats.c
  src/player_hooks.c)
target_include_directories(player_cheats PRIVATE include)

# Windows reads the keyboard through Win32. Linux and macOS read it from
# Foxhollow's own SDL3 runtime, looked up at load time, so no SDL is linked.
# Foxhollow loads mod.dll on Windows and mod.so everywhere else, including macOS,
# where CMake would otherwise name a shared library .dylib.
if(WIN32)
  target_sources(player_cheats PRIVATE src/platform_input_windows.c)
  target_link_libraries(player_cheats PRIVATE user32)
  set(MOD_LIBRARY_NAME "mod.dll")
else()
  target_sources(player_cheats PRIVATE src/platform_input_sdl.c)
  target_link_libraries(player_cheats PRIVATE ${CMAKE_DL_LIBS})
  set_target_properties(player_cheats PROPERTIES SUFFIX ".so")
  set(MOD_LIBRARY_NAME "mod.so")
endif()

# Only the FH_MOD_EXPORT entry points are visible to the loader.
set_target_properties(player_cheats PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mod"
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_VISIBILITY_PRESET hidden)

# The package folder is named after the manifest id, which is also how the
# install and package scripts find it. Editing mod.json re-runs configure.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mod.json")
file(READ "${CMAKE_SOURCE_DIR}/mod.json" MOD_MANIFEST)
string(JSON MOD_ID ERROR_VARIABLE MOD_ID_ERROR GET "${MOD_MANIFEST}" id)
if(MOD_ID_ERROR OR NOT MOD_ID MATCHES "^[A-Za-z0-9][A-Za-z0-9._-]*$" OR MOD_ID MATCHES "\\.$")
  message(FATAL_ERROR "mod.json must define a valid \"id\" (got \"${MOD_ID}\")")
endif()

set(MOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${MOD_ID}")
set(MOD_LIB_DIR "${MOD_OUTPUT_DIR}/lib/${MOD_PLATFORM_DIR}")

add_custom_command(TARGET player_cheats POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_LIB_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:player_cheats>" "${MOD_LIB_DIR}/${MOD_LIBRARY_NAME}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/mod.json" "${MOD_OUTPUT_DIR}/mod.json")
