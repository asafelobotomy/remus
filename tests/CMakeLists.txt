# Enable testing
enable_testing()

# Find Qt6 Test module
find_package(Qt6 REQUIRED COMPONENTS Test)

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

# Display test target
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_constants test_cache_deserialization test_system_name_resolution test_levenshtein_matching
            test_matching_engine test_dat_parser test_template_engine test_multi_file_detection
            test_multi_signal_matching test_pipeline_integration
    COMMENT "Running tests..."
)
