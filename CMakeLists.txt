cmake_minimum_required(VERSION 3.25)
project(player_cheats VERSION 1.0.0 LANGUAGES C)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Player Cheats currently supports Windows x64 only")
endif()

add_library(player_cheats SHARED
  src/mod.c
  src/player_cheats.c
  src/player_hooks.c)
target_include_directories(player_cheats PRIVATE include)
target_link_libraries(player_cheats PRIVATE user32)

set(MOD_PLATFORM_DIR "windows-amd64")
set(MOD_LIBRARY_NAME "mod.dll")

set_target_properties(player_cheats PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mod"
  C_STANDARD 11
  C_STANDARD_REQUIRED ON)

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
