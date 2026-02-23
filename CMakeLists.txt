cmake_minimum_required(VERSION 3.16)
project(Remus VERSION 0.10.1 LANGUAGES CXX)

option(REMUS_ENABLE_CXX20 "Build with C++20 instead of C++17" OFF)
if(REMUS_ENABLE_CXX20)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(REMUS_ENABLE_CCACHE "Use ccache as compiler launcher when available" ON)
if(REMUS_ENABLE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
    else()
        message(STATUS "ccache not found; continuing without compiler cache")
    endif()
endif()

option(REMUS_ENABLE_UNITY_BUILD "Enable CMake unity/jumbo builds" OFF)
if(REMUS_ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
endif()

option(REMUS_ENABLE_PCH "Enable precompiled headers for Remus targets" ON)

option(ENABLE_COVERAGE "Enable coverage flags" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-O0 -g --coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage flags not supported for this compiler")
    endif()
endif()

option(REMUS_BUILD_TUI "Build terminal UI (Notcurses-based)" OFF)

# Avoid noisy Qt optional Vulkan header lookup when Vulkan headers are not installed.
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders TRUE)
endif()

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Sql
    Network
    Concurrent
    Gui
    Quick
    Qml
)

# Find zlib for CRC32 hashing
find_package(ZLIB REQUIRED)

# Project include directory
include_directories(${CMAKE_SOURCE_DIR}/src)

# Core library
add_subdirectory(src/core)

# Metadata library
add_subdirectory(src/metadata)

# Services library (shared business logic)
add_subdirectory(src/services)

# UI library and GUI executable
add_subdirectory(src/ui)

# CLI executable
add_executable(remus-cli
    src/cli/main.cpp
    src/cli/interactive_session.cpp
    src/cli/cli_helpers.cpp
    src/cli/cli_commands_info.cpp
    src/cli/cli_commands_metadata.cpp
    src/cli/cli_commands_match.cpp
    src/cli/cli_commands_verify.cpp
    src/cli/cli_commands_organize.cpp
    src/cli/cli_commands_chd.cpp
    src/cli/cli_commands_export.cpp
)

find_package(Curses REQUIRED)
target_include_directories(remus-cli PRIVATE ${CURSES_INCLUDE_DIR})

target_link_libraries(remus-cli PRIVATE
    remus-core
    remus-metadata
    remus-services
    Qt6::Core
    Qt6::Gui
    ${CURSES_LIBRARIES}
)

if(REMUS_ENABLE_PCH)
    target_precompile_headers(remus-cli PRIVATE
        <QString>
        <QStringList>
        <QList>
        <QMap>
        <QVariant>
        <QFileInfo>
    )
endif()

if(REMUS_BUILD_TUI)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses>=3.0.0)
    add_subdirectory(src/tui)
endif()

# Enable testing
enable_testing()
add_subdirectory(tests)

# Installation rules
install(TARGETS remus-cli
    RUNTIME DESTINATION bin
)
