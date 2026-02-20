add_library(remus-metadata STATIC
    metadata_provider.cpp
    screenscraper_provider.cpp
    thegamesdb_provider.cpp
    igdb_provider.cpp
    hasheous_provider.cpp
    clrmamepro_parser.cpp
    local_database_provider.cpp
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
)

target_include_directories(remus-metadata PUBLIC
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
