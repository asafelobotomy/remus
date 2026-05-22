# ── Reusable GUI controller/model library ───────────────────────────────────
# Compiling these sources into a separate linkable target lets both the GUI
# executable and the test suite link the same translation unit, eliminating
# duplicate builds and preventing compile-definition drift.
add_library(remus-gui-controllers STATIC
    controllers/app_controller.cpp
    controllers/app_controller_orchestrator.cpp
    controllers/scan_controller.cpp
    controllers/hash_controller.cpp
    controllers/match_controller.cpp
    controllers/metadata_editor_controller.cpp
    controllers/artwork_controller.cpp
    controllers/dat_manager_controller.cpp
    controllers/verification_controller.cpp
    controllers/organize_controller.cpp
    controllers/conversion_controller.cpp
    controllers/conversion_controller_tools.cpp
    controllers/export_controller.cpp
    controllers/patch_controller.cpp
    controllers/mod_controller.cpp
    controllers/settings_controller.cpp
    controllers/workflow_controller.cpp
    controllers/workflow_controller_run.cpp
    controllers/secret_store.cpp
    models/file_list_model.cpp
    models/match_list_model.cpp
    models/verification_result_model.cpp
    models/mod_list_model.cpp
)

target_include_directories(remus-gui-controllers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(remus-gui-controllers PUBLIC
    remus-core
    remus-metadata
    remus-services
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Network
)

target_link_libraries(remus-gui-controllers PRIVATE
    Qt6Keychain::Qt6Keychain
)

qt_add_executable(remus-gui
    main.cpp
)

qt_add_qml_module(remus-gui
    URI Remus.Gui
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Sidebar.qml
        qml/components/ProgressCard.qml
        qml/components/MetadataEditor.qml
        qml/components/StageCard.qml
        qml/components/QueueSidebar.qml
        qml/components/InspectorPanel.qml
        qml/views/LibraryView.qml
        qml/views/WorkflowView.qml
        qml/views/UtilitiesView.qml
        qml/views/ScanView.qml
        qml/views/HashView.qml
        qml/views/MetadataView.qml
        qml/views/ArtworkView.qml
        qml/views/DatView.qml
        qml/views/VerifyView.qml
        qml/views/OrganizeView.qml
        qml/views/ConversionView.qml
        qml/views/PatchView.qml
        qml/views/ModView.qml
        qml/views/SettingsView.qml
)

target_include_directories(remus-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(remus-gui PRIVATE
    remus-gui-controllers
    remus-core
    remus-metadata
    remus-services
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::QuickLayouts
    Qt6::QuickDialogs2
)

set_target_properties(remus-gui PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS remus-gui
    RUNTIME DESTINATION bin
)