cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ghosttag_apocalypse_reference C)

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE main.c ghost_protocol.c)
