# ── Static library (screens + widgets) for testability ──────
add_library(remus-tui-core STATIC
    app.cpp
    pipeline.cpp
    launch_screen.cpp
    main_menu_screen.cpp
    match_screen.cpp
    manual_match_overlay.cpp
    organize_screen.cpp
    compressor_screen.cpp
    patch_screen.cpp
    library_screen.cpp
    options_screen.cpp
)

target_include_directories(remus-tui-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(remus-tui-core PUBLIC
    remus-core
    remus-metadata
    remus-services
    Qt6::Core
    Qt6::Sql
    PkgConfig::NOTCURSES
)

# ── Executable ──────────────────────────────────────────────
add_executable(remus-tui main.cpp)
target_link_libraries(remus-tui PRIVATE remus-tui-core)
