add_library(remus-services STATIC
    library_service.cpp
    hash_service.cpp
    match_service.cpp
    conversion_service.cpp
    patch_service.cpp
)

target_include_directories(remus-services PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(remus-services PUBLIC
    remus-core
    Qt6::Core
    Qt6::Concurrent
    Qt6::Sql
)

if(REMUS_ENABLE_PCH)
    target_precompile_headers(remus-services PRIVATE
        <QString>
        <QStringList>
        <QList>
        <QMap>
        <QVariant>
        <QFileInfo>
    )
endif()
