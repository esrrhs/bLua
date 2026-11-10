# FindLua on older CMake versions provides variables only. Normalize that to a
# dependency target; bLua's export must never embed this build's Lua paths.
if(NOT TARGET Lua::Lua)
    add_library(Lua::Lua INTERFACE IMPORTED)
    set_target_properties(Lua::Lua PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
    )
endif()
