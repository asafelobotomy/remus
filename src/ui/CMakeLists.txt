# UI library
add_library(remus-ui STATIC
    models/file_list_model.cpp
    models/match_list_model.cpp
    controllers/library_controller.cpp
    controllers/match_controller.cpp
    controllers/conversion_controller.cpp
    controllers/settings_controller.cpp
    controllers/dat_manager_controller.cpp
    controllers/artwork_controller.cpp
    controllers/metadata_editor_controller.cpp
    controllers/export_controller.cpp
    controllers/verification_controller.cpp
    controllers/patch_controller.cpp
    controllers/processing_controller.cpp
    theme_constants.cpp
)

target_link_libraries(remus-ui PUBLIC
    remus-core
    remus-metadata
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

target_include_directories(remus-ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# GUI executable
qt_add_executable(remus-gui
    main.cpp
    resources.qrc
)

target_link_libraries(remus-gui PRIVATE
    remus-ui
    remus-core
    remus-metadata
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
)

# Installation
install(TARGETS remus-gui
    RUNTIME DESTINATION bin
)
