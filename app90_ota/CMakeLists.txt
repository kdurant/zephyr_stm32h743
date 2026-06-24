cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

execute_process(
  COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} describe --always --dirty --tags
  OUTPUT_VARIABLE APP_GIT_REV
  OUTPUT_STRIP_TRAILING_WHITESPACE
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  ERROR_QUIET
)
execute_process(
  COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} log -1 --format=%cs
  OUTPUT_VARIABLE APP_GIT_DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  ERROR_QUIET
)

if(APP_GIT_REV AND APP_GIT_DATE)
  set(APP_VERSION "${APP_GIT_DATE}_${APP_GIT_REV}")
else()
  set(APP_VERSION "unknown")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app90_ota)

target_compile_definitions(app PRIVATE APP_VERSION_STRING="${APP_VERSION}")

target_sources(app PRIVATE
    src/main.c
    src/frame.c
    src/ota_handler.c
    src/flash_ops.c
    src/log_buf.c
)
