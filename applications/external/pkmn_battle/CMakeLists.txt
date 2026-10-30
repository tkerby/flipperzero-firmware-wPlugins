# Host build only (unit tests, simulators, SameBoy harness). The Flipper
# app is built by ufbt from application.fam and never sees this file.
cmake_minimum_required(VERSION 3.20)
project(FlipperPkMBattle C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

enable_testing()

# ---- pkb_core: the portable core shared with the FAP ----
file(GLOB_RECURSE PKB_CORE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/pkb_core/*.c)
add_library(pkb_core STATIC ${PKB_CORE_SOURCES})
target_include_directories(pkb_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/pkb_core/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/pkb_core/gen)
target_compile_definitions(pkb_core PUBLIC PKB_HOST=1 PKB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(pkb_core PRIVATE -Wall -Wextra -Werror=implicit-function-declaration)
endif()

add_subdirectory(host)
