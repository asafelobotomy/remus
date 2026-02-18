# Constants library (header-only)
add_library(remus-constants INTERFACE)
target_include_directories(remus-constants INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(remus-core STATIC
    scanner.cpp
    system_detector.cpp
    hasher.cpp
    database.cpp
    matching_engine.cpp
    template_engine.cpp
    organize_engine.cpp
    m3u_generator.cpp
    chd_converter.cpp
    archive_extractor.cpp
    archive_creator.cpp
    space_calculator.cpp
    dat_parser.cpp
    header_detector.cpp
    verification_engine.cpp
    patch_engine.cpp
    logging_categories.cpp
    system_resolver.cpp
)

target_link_libraries(remus-core PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Gui
    ZLIB::ZLIB
    remus-constants
)

target_include_directories(remus-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
