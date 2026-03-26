add_library(remus-metadata STATIC
    metadata_provider.cpp
    http_metadata_provider.cpp
    screenscraper_provider.cpp
    screenscraper_provider_parse.cpp
    thegamesdb_provider.cpp
    igdb_provider.cpp
    hasheous_provider.cpp
    hasheous_provider_enrichment.cpp
    clrmamepro_parser.cpp
    local_database_provider.cpp
    local_database_provider_match.cpp
    gametdb_provider.cpp
    libretro_metadata_parser.cpp
    provider_orchestrator.cpp
    metadata_cache.cpp
    rate_limiter.cpp
    artwork_downloader.cpp
    filename_normalizer.cpp
)

target_link_libraries(remus-metadata PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    remus-constants
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
