# Enable testing
enable_testing()

# Find Qt6 Test module
find_package(Qt6 REQUIRED COMPONENTS Test)

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

add_remus_test(test_patched_rom_parser PatchedRomParserTest
    SOURCES test_patched_rom_parser.cpp
    LIBS Qt6::Test Qt6::Core remus-core
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

add_remus_test(test_libretro_metadata_parser LibretroMetadataParserTest
    SOURCES test_libretro_metadata_parser.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata
)

add_remus_test(test_thumbnail_urls ThumbnailUrlTest
    SOURCES test_thumbnail_urls.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata remus-core
)

add_remus_test(test_gametdb_provider GameTDBProviderTest
    SOURCES test_gametdb_provider.cpp
    LIBS Qt6::Test Qt6::Core remus-metadata remus-core
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

add_remus_test(test_wikidata_provider WikidataProviderTest
    SOURCES test_wikidata_provider.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Network remus-metadata remus-core
)

add_remus_test(test_retroachievements_provider RetroAchievementsProviderTest
    SOURCES test_retroachievements_provider.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Network remus-metadata remus-core
)

add_remus_test(test_cli_smoke CliSmokeTest
    SOURCES test_cli_smoke.cpp
    LIBS Qt6::Test Qt6::Core remus-core remus-metadata
)
target_compile_definitions(test_cli_smoke PRIVATE
    REMUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

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

add_remus_test(test_mod_workflow ModWorkflowTest
    SOURCES test_mod_workflow.cpp test_mod_workflow_download.cpp test_mod_workflow_fixture.h
    LIBS Qt6::Test Qt6::Core Qt6::Network Qt6::Sql remus-services remus-core
)
target_compile_definitions(test_mod_workflow PRIVATE
    REMUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ── New coverage tests (2026-02-20) ───────────────────────────────────────

add_remus_test(test_database DatabaseTest
    SOURCES test_database.cpp test_database_files.cpp test_database_relations.cpp test_database_fixture.h
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-core
)

add_remus_test(test_verification_engine VerificationEngineTest
    SOURCES test_verification_engine.cpp test_verification_engine_reports.cpp test_verification_engine_fixture.h
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

add_remus_test(test_rom_bundler RomBundlerTest
    SOURCES test_rom_bundler.cpp test_rom_bundler_disc.cpp test_rom_bundler_fixture.h
    LIBS Qt6::Test Qt6::Core Qt6::Sql remus-core
)

add_remus_test(test_cli_helpers CliHelpersTest
    SOURCES test_cli_helpers.cpp ../src/cli/cli_helpers.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Sql Qt6::Network remus-core remus-metadata remus-constants
)

add_remus_test(test_cli_mod_support CliModSupportTest
    SOURCES test_cli_mod_support.cpp ../src/cli/cli_mod_support.cpp
    LIBS Qt6::Test Qt6::Core Qt6::Network remus-services remus-core
)


add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${REMUS_TEST_TARGETS}
    COMMENT "Running tests..."
)
