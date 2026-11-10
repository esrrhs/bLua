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
option(BLUA_ENABLE_SANITIZERS "Enable ASan/UBSan for bLua tests (GCC/Clang)" OFF)

# Find Lua dependency
find_package(Lua 5.1 REQUIRED)
include(cmake/bLuaLua.cmake)

# Define header-only library target
add_library(bLua INTERFACE)
add_library(bLua::bLua ALIAS bLua)

target_compile_features(bLua INTERFACE cxx_std_17)

include(GNUInstallDirs)

target_include_directories(bLua INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(bLua INTERFACE Lua::Lua)

if(NOT WIN32)
    target_link_libraries(bLua INTERFACE ${CMAKE_DL_LIBS})
endif()

# Tests
if(BLUA_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(blua test/test.cpp test/test_odr.cpp)
        target_link_libraries(blua PRIVATE bLua)

        add_executable(blua_regressions test/regression.cpp)
        target_link_libraries(blua_regressions PRIVATE bLua)
        foreach(test_target blua blua_regressions)
            if(MSVC)
                target_compile_options(${test_target} PRIVATE /W4)
            else()
                target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wpedantic)
            endif()
            if(BLUA_ENABLE_SANITIZERS)
                if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                    message(FATAL_ERROR "BLUA_ENABLE_SANITIZERS needs GCC or Clang")
                endif()
                target_compile_options(${test_target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(${test_target} PRIVATE -fsanitize=address,undefined)
            endif()
        endforeach()

        add_custom_command(TARGET blua POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/test/test.lua
                $<TARGET_FILE_DIR:blua>/test.lua
        )

        add_test(NAME blua_test
            COMMAND blua
            WORKING_DIRECTORY $<TARGET_FILE_DIR:blua>
        )
        add_test(NAME blua_regressions COMMAND blua_regressions)
        add_test(NAME blua_check_failure COMMAND blua --intentional-failure)
        add_test(NAME blua_lua_failure COMMAND blua --intentional-lua-failure)
        set_tests_properties(blua_check_failure blua_lua_failure PROPERTIES WILL_FAIL TRUE)
        if(BLUA_INSTALL)
            configure_file(test/relocate.cmake.in relocate.cmake @ONLY)
            add_test(NAME blua_relocated_consumer
                COMMAND ${CMAKE_COMMAND} "-DTEST_CONFIG=$<CONFIG>" -P ${CMAKE_CURRENT_BINARY_DIR}/relocate.cmake)
        endif()
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
    install(FILES cmake/bLuaLua.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bLua)

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
