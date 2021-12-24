cmake_minimum_required(VERSION 2.8)
project(luabridge)

set(CMAKE_C_STANDARD 14)
set(CMAKE_CXX_STANDARD 14)

include_directories(.)

aux_source_directory(./test LUABRIDGE_SRC_LIST)

add_executable(luabridge ${LUABRIDGE_SRC_LIST})
IF (WIN32)
    target_link_libraries(luabridge lua)
ELSE ()
    target_link_libraries(luabridge lua dl)
ENDIF ()
