cmake_minimum_required(VERSION 2.8)
project(luabridge)

set(CMAKE_C_STANDARD 11)

include_directories(.)

aux_source_directory(./test LUABRIDGE_SRC_LIST)

add_executable(luabridge ${LUABRIDGE_SRC_LIST})
target_link_libraries(luabridge lua)
