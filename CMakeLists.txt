cmake_minimum_required(VERSION 3.15...3.30)
project(bLua
    VERSION 1.0.0
    DESCRIPTION "A lightweight, header-only C++17 Lua bridge library"
    HOMEPAGE_URL "https://github.com/esrrhs/bLua"
    LANGUAGES CXX
)

# Options
option(BLUA_BUILD_TESTS "Build bLua tests" ON)
option(BLUA_INSTALL "Generate install targets" ON)

# Find Lua dependency
find_package(Lua REQUIRED)

# Define header-only library target
add_library(bLua INTERFACE)
add_library(bLua::bLua ALIAS bLua)

target_compile_features(bLua INTERFACE cxx_std_17)

include(GNUInstallDirs)

target_include_directories(bLua INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(TARGET Lua::Lua)
    target_link_libraries(bLua INTERFACE Lua::Lua)
else()
    target_include_directories(bLua INTERFACE ${LUA_INCLUDE_DIR})
    target_link_libraries(bLua INTERFACE ${LUA_LIBRARIES})
endif()

if(NOT WIN32)
    target_link_libraries(bLua INTERFACE ${CMAKE_DL_LIBS})
endif()

# Tests
if(BLUA_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(blua test/test.cpp test/test_odr.cpp)
        target_link_libraries(blua PRIVATE bLua)

        if(MSVC)
            target_compile_options(blua PRIVATE /W4)
        else()
            target_compile_options(blua PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_custom_command(TARGET blua POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/test/test.lua
                $<TARGET_FILE_DIR:blua>/test.lua
        )

        add_test(NAME blua_test
            COMMAND blua
            WORKING_DIRECTORY $<TARGET_FILE_DIR:blua>
        )
    endif()
endif()

# Installation & Export
if(BLUA_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS bLua
        EXPORT bLuaTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(FILES bLua.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT bLuaTargets
        FILE bLuaTargets.cmake
        NAMESPACE bLua::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bLua
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/bLuaConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bLuaConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/bLuaConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bLua
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/bLuaConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/bLuaConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bLua
    )
endif()
