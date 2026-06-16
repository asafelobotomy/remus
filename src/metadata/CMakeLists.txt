add_library(remus-metadata STATIC
    metadata_provider.cpp
    thumbnail_url_helper.cpp
    http_metadata_provider.cpp
    screenscraper_provider.cpp
    screenscraper_provider_parse.cpp
    thegamesdb_provider.cpp
    igdb_provider.cpp
    zxinfo_provider.cpp
    hasheous_provider.cpp
    hasheous_provider_enrichment.cpp
    playmatch_provider.cpp
    clrmamepro_parser.cpp
    local_database_provider.cpp
    local_database_provider_lookup.cpp
    local_database_provider_match.cpp
    local_database_provider_thumbnails.cpp
    gametdb_provider.cpp
    gametdb_provider_artwork.cpp
    retroachievements_provider.cpp
    wikidata_provider.cpp
    libretro_metadata_parser.cpp
    provider_orchestrator.cpp
    provider_orchestrator_enrich.cpp
    provider_orchestrator_fallback.cpp
    metadata_cache.cpp
    rate_limiter.cpp
    metadata_rate_limits.cpp
    artwork_downloader.cpp
    filename_normalizer.cpp
    # Compendium compiler pipeline
    compendium_dat_extractor.cpp
    compendium_normalizer.cpp
    compendium_identity_linker.cpp
    compendium_fact_inserter.cpp
    compendium_merge_resolver.cpp
    compendium_compiler_service.cpp
    compendium_provider.cpp
    compendium_provider_lookup.cpp
    compendium_provider_match.cpp
    compendium_provider_fetch.cpp
    compendium_patch_catalog_importer.cpp
    romhacking_scraper.cpp
)

target_link_libraries(remus-metadata PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    remus-constants
    remus-core
)

target_include_directories(remus-metadata PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(REMUS_ENABLE_PCH)
    target_precompile_headers(remus-metadata PRIVATE
        <QString>
        <QStringList>
        <QList>
        <QMap>
        <QVariant>
        <QFileInfo>
    )
endif()
