cmake_minimum_required(VERSION 3.20)
project(DiscordPresenceRE2R LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output DLL
add_library(DiscordPresenceRE2R SHARED
    src/main.cpp
)

target_compile_definitions(DiscordPresenceRE2R PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Remove the "lib" prefix on Windows
set_target_properties(DiscordPresenceRE2R PROPERTIES
    PREFIX ""
    OUTPUT_NAME "DiscordPresenceRE2R"
)

# Optimized release build flags for MSVC
if(MSVC)
    target_compile_options(DiscordPresenceRE2R PRIVATE
        $<$<CONFIG:Release>:/O2 /GL /GS->
    )
    target_link_options(DiscordPresenceRE2R PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
endif()

# Optional: install directory (can be overridden via command line)
set(GAME_PLUGINS_DIR
    ""
    CACHE PATH "REFramework plugins directory (e.g. C:/path/to/game/reframework/plugins)"
)

if(NOT GAME_PLUGINS_DIR STREQUAL "")
    install(TARGETS DiscordPresenceRE2R
        RUNTIME DESTINATION "${GAME_PLUGINS_DIR}"
    )
    message(STATUS "Install target -> ${GAME_PLUGINS_DIR}")
endif()
