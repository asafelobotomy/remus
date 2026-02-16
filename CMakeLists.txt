cmake_minimum_required(VERSION 3.16)
project(Remus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ENABLE_COVERAGE "Enable coverage flags" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-O0 -g --coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage flags not supported for this compiler")
    endif()
endif()

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Sql
    Network
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

# UI library and GUI executable
add_subdirectory(src/ui)

# CLI executable
add_executable(remus-cli
    src/cli/main.cpp
)

target_link_libraries(remus-cli PRIVATE
    remus-core
    remus-metadata
    Qt6::Core
    Qt6::Gui
)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Installation rules
install(TARGETS remus-cli
    RUNTIME DESTINATION bin
)
