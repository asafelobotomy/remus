# Enable testing
enable_testing()

# Find Qt6 Test module
find_package(Qt6 REQUIRED COMPONENTS Test)
find_package(Curses REQUIRED)

# Helper to reduce repetitive test target boilerplate.
set(REMUS_TEST_TARGETS)
function(add_remus_test target_name ctest_name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LIBS)
    cmake_parse_arguments(ART "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ART_SOURCES)
        set(ART_SOURCES "${target_name}.cpp")
    endif()

    add_executable(${target_name} ${ART_SOURCES})
    target_link_libraries(${target_name} PRIVATE ${ART_LIBS})
    add_test(NAME ${ctest_name} COMMAND ${target_name})

    list(APPEND REMUS_TEST_TARGETS ${target_name})
    set(REMUS_TEST_TARGETS "${REMUS_TEST_TARGETS}" PARENT_SCOPE)
endfunction()

add_remus_test(test_constants ConstantsTest
    SOURCES test_constants.cpp
    LIBS Qt6::Test remus-constants
)

add_remus_test(test_cache_deserialization CacheDeserializationTest
    SOURCES test_cache_deserialization.cpp
    LIBS Qt6::Core remus-core remus-metadata
)

add_remus_test(test_system_name_resolution SystemNameResolutionTest
    SOURCES test_system_name_resolution.cpp
    LIBS Qt6::Core Qt6::Sql remus-core
)

add_remus_test(test_levenshtein_matching LevenshteinMatchingTest
    SOURCES test_levenshtein_matching.cpp
    LIBS Qt6::Core
)

add_remus_test(test_matching_engine MatchingEngineTest
    SOURCES test_matching_engine.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_dat_parser DatParserTest
    SOURCES test_dat_parser.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_template_engine TemplateEngineTest
    SOURCES test_template_engine.cpp
    LIBS Qt6::Test Qt6::Core remus-core remus-constants
)

add_remus_test(test_multi_file_detection MultiFileDetectionTest
    SOURCES test_multi_file_detection.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_multi_signal_matching MultiSignalMatchingTest
    SOURCES test_multi_signal_matching.cpp
    LIBS Qt6::Core remus-metadata remus-core
)

add_remus_test(test_pipeline_integration PipelineIntegrationTest
    SOURCES test_pipeline_integration.cpp
    LIBS Qt6::Core Qt6::Sql remus-metadata remus-core
)

add_remus_test(test_hasher HasherTest
    SOURCES test_hasher.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_header_detector HeaderDetectorTest
    SOURCES test_header_detector.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_system_resolver SystemResolverTest
    SOURCES test_system_resolver.cpp
    LIBS Qt6::Test Qt6::Core remus-core remus-constants
)

add_remus_test(test_settings_controller SettingsControllerTest
    SOURCES test_settings_controller.cpp
    LIBS Qt6::Test Qt6::Core remus-ui
)

add_remus_test(test_library_controller LibraryControllerTest
    SOURCES test_library_controller.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-ui remus-core
)

add_remus_test(test_chd_converter ChdConverterTest
    SOURCES test_chd_converter.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_archive_extractor ArchiveExtractorTest
    SOURCES test_archive_extractor.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_patch_engine PatchEngineTest
    SOURCES test_patch_engine.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_space_calculator SpaceCalculatorTest
    SOURCES test_space_calculator.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_rate_limiter RateLimiterTest
    SOURCES test_rate_limiter.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata remus-core
)

add_remus_test(test_metadata_provider MetadataProviderTest
    SOURCES test_metadata_provider.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Network remus-metadata remus-core
)

add_remus_test(test_hasheous_parsing HasheousParsingTest
    SOURCES test_hasheous_parsing.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata remus-core
)

add_remus_test(test_clrmamepro_parser ClrMameProParserTest
    SOURCES test_clrmamepro_parser.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata
)

add_remus_test(test_filename_normalizer FilenameNormalizerTest
    SOURCES test_filename_normalizer.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata
)

add_remus_test(test_metadata_cache MetadataCacheTest
    SOURCES test_metadata_cache.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-metadata remus-core
)

add_remus_test(test_provider_orchestrator ProviderOrchestratorTest
    SOURCES test_provider_orchestrator.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata remus-core
)

add_remus_test(test_hash_algorithms HashAlgorithmsTest
    SOURCES test_hash_algorithms.cpp
    LIBS Qt6::Test Qt6::Core remus-constants
)

add_remus_test(test_scanner ScannerTest
    SOURCES test_scanner.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_artwork_downloader ArtworkDownloaderTest
    SOURCES test_artwork_downloader.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Network remus-metadata remus-core
)

add_remus_test(test_providers_minimal ProvidersMinimalTest
    SOURCES test_providers_minimal.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata remus-core
)

add_remus_test(test_cli_smoke CliSmokeTest
    SOURCES test_cli_smoke.cpp
    LIBS Qt6::Test Qt6::Core remus-core remus-metadata
)

add_remus_test(test_session_state SessionStateTest
    SOURCES test_session_state.cpp ../src/cli/interactive_session.cpp
    LIBS Qt6::Test Qt6::Core remus-core ${CURSES_LIBRARIES}
)

add_remus_test(test_theme_constants ThemeConstantsTest
    SOURCES test_theme_constants.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Gui remus-ui
)

add_remus_test(test_dat_manager_controller DatManagerControllerTest
    SOURCES test_dat_manager_controller.cpp
    LIBS Qt6::Test Qt6::Core remus-ui
)

# ── TUI Widget tests (Phase 6.2+) ─────────────────────────────────
if(TARGET remus-tui-core)
    add_remus_test(test_tui_widgets TuiWidgetsTest
        SOURCES test_tui_widgets.cpp
        LIBS Qt6::Test remus-tui-core
    )

    add_remus_test(test_library_screen LibraryScreenTest
        SOURCES test_library_screen.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )

    add_remus_test(test_tui_pipeline TuiPipelineTest
        SOURCES test_tui_pipeline.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )

    add_remus_test(test_match_screen MatchScreenTest
        SOURCES test_match_screen.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )

    add_remus_test(test_organize_screen OrganizeScreenTest
        SOURCES test_organize_screen.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )

    add_remus_test(test_compressor_screen CompressorScreenTest
        SOURCES test_compressor_screen.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )

    add_remus_test(test_patch_screen PatchScreenTest
        SOURCES test_patch_screen.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )

    add_remus_test(test_options_screen OptionsScreenTest
        SOURCES test_options_screen.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )

    add_remus_test(test_tool_hints ToolHintsTest
        SOURCES test_tool_hints.cpp
        LIBS Qt6::Test remus-tui-core
    )

    add_remus_test(test_tui_smoke TuiSmokeTest
        SOURCES test_tui_smoke.cpp
        LIBS Qt6::Test Qt6::Sql remus-tui-core
    )
endif()

# ── Service layer tests (Phase 6.4) ───────────────────────────────
add_remus_test(test_library_service LibraryServiceTest
    SOURCES test_library_service.cpp
    LIBS Qt6::Test Qt6::Sql remus-services remus-core
)

add_remus_test(test_match_service MatchServiceTest
    SOURCES test_match_service.cpp
    LIBS Qt6::Test Qt6::Sql remus-services remus-core
)

add_remus_test(test_conversion_service ConversionServiceTest
    SOURCES test_conversion_service.cpp
    LIBS Qt6::Test remus-services remus-core
)

add_remus_test(test_patch_service PatchServiceTest
    SOURCES test_patch_service.cpp
    LIBS Qt6::Test remus-services remus-core
)

add_remus_test(test_hash_service HashServiceTest
    SOURCES test_hash_service.cpp
    LIBS Qt6::Test Qt6::Sql remus-services remus-core
)

# ── New coverage tests (2026-02-20) ───────────────────────────────────────

add_remus_test(test_database DatabaseTest
    SOURCES test_database.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-core
)

add_remus_test(test_verification_engine VerificationEngineTest
    SOURCES test_verification_engine.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-core
)

add_remus_test(test_organize_engine OrganizeEngineTest
    SOURCES test_organize_engine.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-core remus-metadata
)

add_remus_test(test_archive_creator ArchiveCreatorTest
    SOURCES test_archive_creator.cpp
    LIBS Qt6::Test Qt6::Core remus-core
)

add_remus_test(test_m3u_generator M3UGeneratorTest
    SOURCES test_m3u_generator.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-core
)

add_remus_test(test_cli_helpers CliHelpersTest
    SOURCES test_cli_helpers.cpp ../src/cli/cli_helpers.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql Qt6::Network remus-core remus-metadata remus-constants
)

add_remus_test(test_processing_controller ProcessingControllerTest
    SOURCES test_processing_controller.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql Qt6::Network remus-ui remus-core remus-metadata
)

add_remus_test(test_export_controller ExportControllerTest
    SOURCES test_export_controller.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql Qt6::Network remus-ui remus-core
)


add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${REMUS_TEST_TARGETS}
    COMMENT "Running tests..."
)
