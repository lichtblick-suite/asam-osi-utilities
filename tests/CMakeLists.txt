# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
# Use GTest from vcpkg (or system)
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

file(GLOB_RECURSE test_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(FILTER test_sources EXCLUDE REGEX "/intentional-failures/")

add_executable(
        unit_tests
        ${test_sources}
)

target_compile_features(unit_tests PRIVATE cxx_std_17)
set_target_properties(unit_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Link to the OSIUtilities library instead of recompiling all sources
target_link_libraries(unit_tests PRIVATE OSIUtilities)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main)

# see src/CMakeLists.txt
target_compile_definitions(unit_tests PRIVATE OSI_TRACE_FILE_SPEC_VERSION="${OSI_TRACE_FILE_SPEC_VERSION}")

# Test data directory for fixture-based tests
target_compile_definitions(unit_tests PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

# Link against OSI made available by parent CMakeLists.txt
if (LINK_WITH_SHARED_OSI)
    target_link_libraries(unit_tests PRIVATE open_simulation_interface::open_simulation_interface)
else ()
    target_link_libraries(unit_tests PRIVATE open_simulation_interface::open_simulation_interface_pic)
endif ()

# Find compression libraries (LZ4, ZSTD)
include(FindCompression)
find_compression_libraries()

if (COMPRESSION_FOUND)
    target_link_libraries(unit_tests PRIVATE ${LZ4_TARGET} ${ZSTD_TARGET})
else ()
    message(FATAL_ERROR "Could not find LZ4 and ZSTD compression libraries")
endif ()

# include public headers of the library
target_include_directories(unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
gtest_discover_tests(unit_tests)   # Register the tests to gtest

# ============================================================================
# Integration tests for example executables
# ============================================================================
find_program(BASH NAMES bash)

if (BUILD_EXAMPLES)
    # Writer integration tests — verify they run to completion
    add_test(NAME integration_example_mcap_writer
        COMMAND example_mcap_writer)
    add_test(NAME integration_example_binary_writer
        COMMAND example_single_channel_binary_writer)
    add_test(NAME integration_example_txth_writer
        COMMAND example_txth_writer)

    # Reader/converter integration tests — need bash interpreter
    if (BASH)
        add_test(NAME integration_mcap_reader
            COMMAND ${BASH} ${CMAKE_CURRENT_SOURCE_DIR}/integration/run_reader_integration.sh
                $<TARGET_FILE:example_mcap_writer>
                $<TARGET_FILE:example_mcap_reader>
                ${CMAKE_BINARY_DIR}/tmp)

        add_test(NAME integration_binary_reader
            COMMAND ${BASH} ${CMAKE_CURRENT_SOURCE_DIR}/integration/run_reader_integration.sh
                $<TARGET_FILE:example_single_channel_binary_writer>
                $<TARGET_FILE:example_single_channel_binary_reader>
                ${CMAKE_BINARY_DIR}/tmp
                --type SensorView)

        add_test(NAME integration_txth_reader
            COMMAND ${BASH} ${CMAKE_CURRENT_SOURCE_DIR}/integration/run_reader_integration.sh
                $<TARGET_FILE:example_txth_writer>
                $<TARGET_FILE:example_txth_reader>
                ${CMAKE_BINARY_DIR}/tmp
                --type SensorView)

        # Converter integration test — binary writer → converter → mcap reader
        add_test(NAME integration_convert_osi2mcap
            COMMAND ${BASH} ${CMAKE_CURRENT_SOURCE_DIR}/integration/run_converter_integration.sh
                $<TARGET_FILE:example_single_channel_binary_writer>
                $<TARGET_FILE:convert_osi2mcap>
                $<TARGET_FILE:example_mcap_reader>
                ${CMAKE_BINARY_DIR}/tmp
                --input-type SensorView)
    else ()
        message(STATUS "bash not found — skipping shell-based integration tests")
    endif ()

endif ()
