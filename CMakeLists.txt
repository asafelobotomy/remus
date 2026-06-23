cmake_minimum_required(VERSION 3.16)
project(Remus VERSION 0.12.0 LANGUAGES CXX)

option(REMUS_ENABLE_CXX20 "Build with C++20 instead of C++17" OFF)
if(REMUS_ENABLE_CXX20)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

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
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_compile_options(-fprofile-update=atomic)
        endif()
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage flags not supported for this compiler")
    endif()
endif()

option(REMUS_ENABLE_WARNINGS "Enable -Wall -Wextra compiler warnings (recommended for dev/CI builds)" OFF)
if(REMUS_ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # -Wno-unused-parameter: Qt signal/slot signatures legitimately omit names.
        add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    elseif(MSVC)
        add_compile_options(/W3)
    endif()
endif()


# Avoid noisy Qt optional Vulkan header lookup when Vulkan headers are not installed.
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders TRUE)
endif()

# Workaround: qt6-svg 6.11 ships cmake files that reference a function
# not present in qt6-base 6.10.x. Define a stub so plugin discovery
# does not abort.  Remove once qt6-base ≥ 6.11 is installed.
if(NOT COMMAND _qt_internal_should_include_targets)
    function(_qt_internal_should_include_targets)
        cmake_parse_arguments(arg "" "OUT_VAR_SHOULD_SKIP" "TARGETS;NAMESPACE;PROJECT_NAME" ${ARGN})
        set(${arg_OUT_VAR_SHOULD_SKIP} FALSE PARENT_SCOPE)
    endfunction()
endif()

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Sql Network Concurrent Gui Quick Qml QuickControls2 QuickLayouts QuickDialogs2)

# OS-backed credential store (Secret Service on Linux, Keychain on macOS)
find_package(Qt6Keychain REQUIRED)

# Find zlib for CRC32 hashing
find_package(ZLIB REQUIRED)

# Find libarchive for archive handling (ZIP, tar, etc.)
find_package(LibArchive REQUIRED)

# Core library
add_subdirectory(src/core)

# Metadata library
add_subdirectory(src/metadata)

# Services library (shared business logic)
add_subdirectory(src/services)

# GUI executable
add_subdirectory(src/gui)

# CLI executable
set(REMUS_CLI_SOURCES
    src/cli/main.cpp
    src/cli/cli_helpers.cpp
    src/cli/cli_helpers_providers.cpp
    src/cli/cli_options.cpp
    src/cli/cli_mod_support.cpp
    src/cli/cli_mod_support_format.cpp
    src/cli/cli_commands_info.cpp
    src/cli/cli_commands_process.cpp
    src/cli/cli_commands_metadata.cpp
    src/cli/cli_commands_enrich.cpp
    src/cli/cli_commands_match.cpp
    src/cli/cli_commands_verify.cpp
    src/cli/cli_commands_organize.cpp
    src/cli/cli_commands_bundle.cpp
    src/cli/cli_commands_chd.cpp
    src/cli/cli_commands_convert.cpp
    src/cli/cli_commands_export.cpp
    src/cli/cli_commands_mods.cpp
    src/cli/cli_commands_mod_catalog.cpp
    src/cli/cli_commands_dat.cpp
    src/cli/cli_commands_compendium.cpp
    src/cli/cli_compendium_build_phases.cpp
    src/cli/cli_commands_compendium_dedup.cpp
    src/cli/cli_commands_compendium_patch.cpp
    src/cli/cli_commands_compendium_enrich.cpp
    src/cli/cli_commands_compendium_ingest.cpp
    src/cli/cli_commands_compendium_backfill.cpp
    src/cli/cli_commands_compendium_coverage.cpp
    src/cli/compendium_enrichment.cpp
    src/cli/compendium_enrichment_gametdb.cpp
    src/cli/compendium_enrichment_igdb.cpp
    src/cli/compendium_enrichment_ra.cpp
    src/cli/compendium_enrichment_hasheous.cpp
    src/cli/compendium_enrichment_playmatch.cpp
    src/cli/compendium_enrichment_openvgdb.cpp
    src/cli/compendium_enrichment_mame.cpp
    src/cli/compendium_enrichment_mame_listxml.cpp
    src/cli/compendium_enrichment_sql.cpp
    src/cli/compendium_enrichment_zxinfo.cpp
    src/cli/compendium_enrichment_screenscraper.cpp
    src/cli/compendium_enrichment_wikidata.cpp
    src/cli/compendium_enrichment_launchbox.cpp
    src/cli/compendium_enrichment_thegamesdb.cpp
    src/cli/compendium_enrichment_thumbnails.cpp
    src/cli/compendium_consolidate_thumbnails.cpp
    src/cli/compendium_cwebp_resolver.cpp
    src/cli/compendium_artwork_transcode.cpp
    src/cli/compendium_artwork_blob_store.cpp
    src/cli/cli_commands_compendium_thumbnails.cpp
)

add_executable(remus-cli ${REMUS_CLI_SOURCES})

target_link_libraries(remus-cli PRIVATE
    remus-core
    remus-metadata
    remus-services
    Qt6::Core
    Qt6::Gui
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

# Enable testing
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Installation rules
install(TARGETS remus-cli
    RUNTIME DESTINATION bin
)
