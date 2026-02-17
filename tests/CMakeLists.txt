# Enable testing
enable_testing()

# Find Qt6 Test module
find_package(Qt6 REQUIRED COMPONENTS Test)
find_package(Curses REQUIRED)

# Constants library tests
add_executable(test_constants test_constants.cpp)
target_link_libraries(test_constants PRIVATE
    Qt6::Test
    remus-constants
)

# Add test to CTest
add_test(NAME ConstantsTest COMMAND test_constants)

# Metadata cache tests
add_executable(test_cache_deserialization test_cache_deserialization.cpp)
target_link_libraries(test_cache_deserialization PRIVATE
    Qt6::Core
    remus-core
    remus-metadata
)

add_test(NAME CacheDeserializationTest COMMAND test_cache_deserialization)

# System name resolution test
add_executable(test_system_name_resolution test_system_name_resolution.cpp)
target_link_libraries(test_system_name_resolution PRIVATE
    Qt6::Core
    Qt6::Sql
    remus-core
)

add_test(NAME SystemNameResolutionTest COMMAND test_system_name_resolution)

# Levenshtein distance matching test
add_executable(test_levenshtein_matching test_levenshtein_matching.cpp)
target_link_libraries(test_levenshtein_matching PRIVATE
    Qt6::Core
)

add_test(NAME LevenshteinMatchingTest COMMAND test_levenshtein_matching)

# Matching engine tests (P3)
add_executable(test_matching_engine test_matching_engine.cpp)
target_link_libraries(test_matching_engine PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME MatchingEngineTest COMMAND test_matching_engine)

# DAT parser tests (P3)
add_executable(test_dat_parser test_dat_parser.cpp)
target_link_libraries(test_dat_parser PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME DatParserTest COMMAND test_dat_parser)

# Template engine tests (P3)
add_executable(test_template_engine test_template_engine.cpp)
target_link_libraries(test_template_engine PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
    remus-constants
)

add_test(NAME TemplateEngineTest COMMAND test_template_engine)

# Multi-file detection tests (P3)
add_executable(test_multi_file_detection test_multi_file_detection.cpp)
target_link_libraries(test_multi_file_detection PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME MultiFileDetectionTest COMMAND test_multi_file_detection)

# Multi-signal matching tests (Phase 3)
add_executable(test_multi_signal_matching test_multi_signal_matching.cpp)
target_link_libraries(test_multi_signal_matching PRIVATE
    Qt6::Core
    remus-metadata
    remus-core
)

add_test(NAME MultiSignalMatchingTest COMMAND test_multi_signal_matching)

# Pipeline integration test
add_executable(test_pipeline_integration test_pipeline_integration.cpp)
target_link_libraries(test_pipeline_integration PRIVATE
    Qt6::Core
    Qt6::Sql
    remus-metadata
    remus-core
)

add_test(NAME PipelineIntegrationTest COMMAND test_pipeline_integration)

# Hasher tests
add_executable(test_hasher test_hasher.cpp)
target_link_libraries(test_hasher PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME HasherTest COMMAND test_hasher)

# Header detector tests
add_executable(test_header_detector test_header_detector.cpp)
target_link_libraries(test_header_detector PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME HeaderDetectorTest COMMAND test_header_detector)

# System resolver tests
add_executable(test_system_resolver test_system_resolver.cpp)
target_link_libraries(test_system_resolver PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
    remus-constants
)

add_test(NAME SystemResolverTest COMMAND test_system_resolver)

# Settings controller tests
add_executable(test_settings_controller test_settings_controller.cpp)
target_link_libraries(test_settings_controller PRIVATE
    Qt6::Test
    Qt6::Core
    remus-ui
)

add_test(NAME SettingsControllerTest COMMAND test_settings_controller)

# Library controller tests
add_executable(test_library_controller test_library_controller.cpp)
target_link_libraries(test_library_controller PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Sql
    remus-ui
    remus-core
)

add_test(NAME LibraryControllerTest COMMAND test_library_controller)

# CHD converter tests
add_executable(test_chd_converter test_chd_converter.cpp)
target_link_libraries(test_chd_converter PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME ChdConverterTest COMMAND test_chd_converter)

# Archive extractor tests
add_executable(test_archive_extractor test_archive_extractor.cpp)
target_link_libraries(test_archive_extractor PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME ArchiveExtractorTest COMMAND test_archive_extractor)

# Patch engine tests
add_executable(test_patch_engine test_patch_engine.cpp)
target_link_libraries(test_patch_engine PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME PatchEngineTest COMMAND test_patch_engine)

# Space calculator tests
add_executable(test_space_calculator test_space_calculator.cpp)
target_link_libraries(test_space_calculator PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME SpaceCalculatorTest COMMAND test_space_calculator)

# Rate limiter tests
add_executable(test_rate_limiter test_rate_limiter.cpp)
target_link_libraries(test_rate_limiter PRIVATE
    Qt6::Test
    Qt6::Core
    remus-metadata
    remus-core
)

add_test(NAME RateLimiterTest COMMAND test_rate_limiter)

# Metadata provider tests
add_executable(test_metadata_provider test_metadata_provider.cpp)
target_link_libraries(test_metadata_provider PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Network
    remus-metadata
    remus-core
)

add_test(NAME MetadataProviderTest COMMAND test_metadata_provider)

# Hasheous parsing tests
add_executable(test_hasheous_parsing test_hasheous_parsing.cpp)
target_link_libraries(test_hasheous_parsing PRIVATE
    Qt6::Test
    Qt6::Core
    remus-metadata
    remus-core
)

add_test(NAME HasheousParsingTest COMMAND test_hasheous_parsing)

# Filename normalizer tests
add_executable(test_filename_normalizer test_filename_normalizer.cpp)
target_link_libraries(test_filename_normalizer PRIVATE
    Qt6::Test
    Qt6::Core
    remus-metadata
)

add_test(NAME FilenameNormalizerTest COMMAND test_filename_normalizer)

# Metadata cache tests
add_executable(test_metadata_cache test_metadata_cache.cpp)
target_link_libraries(test_metadata_cache PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Sql
    remus-metadata
    remus-core
)

add_test(NAME MetadataCacheTest COMMAND test_metadata_cache)

# Provider orchestrator tests
add_executable(test_provider_orchestrator test_provider_orchestrator.cpp)
target_link_libraries(test_provider_orchestrator PRIVATE
    Qt6::Test
    Qt6::Core
    remus-metadata
    remus-core
)

add_test(NAME ProviderOrchestratorTest COMMAND test_provider_orchestrator)

# Hash algorithms tests
add_executable(test_hash_algorithms test_hash_algorithms.cpp)
target_link_libraries(test_hash_algorithms PRIVATE
    Qt6::Test
    Qt6::Core
    remus-constants
)

add_test(NAME HashAlgorithmsTest COMMAND test_hash_algorithms)

# Scanner tests
add_executable(test_scanner test_scanner.cpp)
target_link_libraries(test_scanner PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
)

add_test(NAME ScannerTest COMMAND test_scanner)

# Artwork downloader tests
add_executable(test_artwork_downloader test_artwork_downloader.cpp)
target_link_libraries(test_artwork_downloader PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Network
    remus-metadata
    remus-core
)

add_test(NAME ArtworkDownloaderTest COMMAND test_artwork_downloader)

# Provider minimal-path tests
add_executable(test_providers_minimal test_providers_minimal.cpp)
target_link_libraries(test_providers_minimal PRIVATE
    Qt6::Test
    Qt6::Core
    remus-metadata
    remus-core
)

add_test(NAME ProvidersMinimalTest COMMAND test_providers_minimal)

# CLI smoke tests
add_executable(test_cli_smoke test_cli_smoke.cpp)
target_link_libraries(test_cli_smoke PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
    remus-metadata
)

add_test(NAME CliSmokeTest COMMAND test_cli_smoke)

# Session state persistence tests
add_executable(test_session_state
    test_session_state.cpp
    ../src/cli/interactive_session.cpp
)
target_link_libraries(test_session_state PRIVATE
    Qt6::Test
    Qt6::Core
    remus-core
    ${CURSES_LIBRARIES}
)

add_test(NAME SessionStateTest COMMAND test_session_state)

# Theme constants tests
add_executable(test_theme_constants test_theme_constants.cpp)
target_link_libraries(test_theme_constants PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Gui
    remus-ui
)

add_test(NAME ThemeConstantsTest COMMAND test_theme_constants)

# DAT manager controller tests
add_executable(test_dat_manager_controller test_dat_manager_controller.cpp)
target_link_libraries(test_dat_manager_controller PRIVATE
    Qt6::Test
    Qt6::Core
    remus-ui
)

add_test(NAME DatManagerControllerTest COMMAND test_dat_manager_controller)

# Display test target
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_constants test_cache_deserialization test_system_name_resolution test_levenshtein_matching
            test_matching_engine test_dat_parser test_template_engine test_multi_file_detection
            test_multi_signal_matching test_pipeline_integration test_hasher test_header_detector
            test_system_resolver test_settings_controller test_library_controller test_chd_converter
            test_archive_extractor test_patch_engine test_space_calculator test_rate_limiter
            test_metadata_provider test_filename_normalizer test_metadata_cache test_provider_orchestrator
            test_hash_algorithms test_scanner test_artwork_downloader test_providers_minimal
            test_theme_constants test_dat_manager_controller test_hasheous_parsing
    COMMENT "Running tests..."
)
