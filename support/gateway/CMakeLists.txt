cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ghosttag_gateway C)

if(NOT DEFINED GHOST_TAG_COUNT)
  set(GHOST_TAG_COUNT 12)
endif()
if(NOT DEFINED GHOST_TAG_ID_BASE)
  set(GHOST_TAG_ID_BASE 0)
endif()
if(NOT DEFINED GHOST_SECTOR)
  set(GHOST_SECTOR 0)
endif()

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../reference/firmware/include
)
target_sources(app PRIVATE
  main.c
  ${CMAKE_CURRENT_LIST_DIR}/../../reference/firmware/ghost_protocol.c
)
target_compile_definitions(app PRIVATE
  GHOST_TAG_COUNT=${GHOST_TAG_COUNT}
  GHOST_TAG_ID_BASE=${GHOST_TAG_ID_BASE}
  GHOST_SECTOR=${GHOST_SECTOR}
)
