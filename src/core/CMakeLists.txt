# Constants library
add_library(remus-constants STATIC
    systems.cpp
    systems_defs.cpp
    systems_extensions.cpp
)
target_include_directories(remus-constants PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(remus-constants PUBLIC
    Qt6::Core
)

add_library(remus-core STATIC
    scanner.cpp
    scanner_multifile.cpp
    conversion_planner.cpp
    conversion_planner_plan.cpp
    system_detector.cpp
    disc_magic_detector.cpp
    patched_rom_parser.cpp
    match_utils.cpp
    hasher.cpp
    ra_hasher.cpp
    database.cpp
    database_files.cpp
    database_disc_sets.cpp
    database_file_state.cpp
    database_games.cpp
    database_migrations.cpp
    database_migrations_systems.cpp
    database_patches.cpp
    database_schema.cpp
    matching_engine.cpp
    title_similarity.cpp
    template_engine.cpp
    organize_engine.cpp
    organize_engine_undo.cpp
    rom_bundler.cpp
    rom_bundler_bundle.cpp
    rom_bundler_bundle_helpers.cpp
    rom_bundler_staged.cpp
    m3u_generator.cpp
    disc_set_utils.cpp
    disc_title_parser.cpp
    disc_set_key.cpp
    chd_converter.cpp
    chd_header.cpp
    hasheous_config.cpp
    rvz_converter.cpp
    cso_converter.cpp
    wbfs_converter.cpp
    pbp_exporter.cpp
    disc_converter.cpp
    external_tool_runner.cpp
    archive_extractor.cpp
    archive_extractor_extract.cpp
    archive_extractor_info.cpp
    archive_creator.cpp
    archive_creator_compress.cpp
    space_calculator.cpp
    dat_parser.cpp
    header_detector.cpp
    verification_engine.cpp
    verification_engine_verify.cpp
    verification_engine_report.cpp
    verification_hash_matcher.cpp
    verification_storage.cpp
    verification_storage_schema.cpp
    verification_storage_dat.cpp
    verification_engine_disc_sets.cpp
    compendium_disc_bridge.cpp
    compendium_manifest_parser.cpp
    patch_engine.cpp
    patch_engine_apply.cpp
    library_exporter.cpp
    logging_categories.cpp
    system_resolver.cpp
    system_resolver_dat.cpp
    system_resolver_provider_mappings.cpp
)

target_link_libraries(remus-core PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Gui
    Qt6::Concurrent
    ZLIB::ZLIB
    LibArchive::LibArchive
    remus-constants
)

target_include_directories(remus-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(REMUS_ENABLE_PCH)
    target_precompile_headers(remus-core PRIVATE
        <QString>
        <QStringList>
        <QList>
        <QMap>
        <QVariant>
        <QFileInfo>
    )
endif()
