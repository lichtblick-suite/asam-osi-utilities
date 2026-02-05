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
