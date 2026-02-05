# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
# Add subdirectories for organized modules
include_directories(tracefile)

# specify library source files
set(OSIUtilities_SRCS
        tracefile/reader/Reader.cpp
        tracefile/MCAPImplementation.cpp
        tracefile/reader/SingleChannelBinaryTraceFileReader.cpp
        tracefile/writer/SingleChannelBinaryTraceFileWriter.cpp
        tracefile/reader/TXTHTraceFileReader.cpp
        tracefile/writer/TXTHTraceFileWriter.cpp
        tracefile/reader/MCAPTraceFileReader.cpp
        tracefile/writer/MCAPTraceFileWriter.cpp
)

# Create a library target for the entire library
add_library(OSIUtilities ${OSIUtilities_SRCS})

target_compile_features(OSIUtilities PUBLIC cxx_std_17)
set_target_properties(OSIUtilities PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# add a compile definition to specify the version of the trace file specification.
# This is not the version of the linked OSI library, but the version of the trace file spec
# which is implemented (in functions, methods etc.) by this library (OSIUtilities).
target_compile_definitions(OSIUtilities PRIVATE OSI_TRACE_FILE_SPEC_VERSION="${OSI_TRACE_FILE_SPEC_VERSION}")


# Specify the public headers for the library for clean access
target_include_directories(OSIUtilities PUBLIC ${PROJECT_SOURCE_DIR}/include)

# MCAP is header-only with a single implementation TU. On Windows, its default
# visibility uses dllimport/dllexport which causes LNK4217/LNK4286 warnings
# when linking statically. Override MCAP_PUBLIC to empty for static builds.
if (WIN32)
    target_compile_definitions(OSIUtilities PUBLIC MCAP_PUBLIC=)
endif ()


# Link against OSI made available by parent CMakeLists.txt
if (LINK_WITH_SHARED_OSI)
    target_link_libraries(OSIUtilities PRIVATE open_simulation_interface::open_simulation_interface)
else ()
    target_link_libraries(OSIUtilities PRIVATE open_simulation_interface::open_simulation_interface_pic)
endif ()


# get mcap and its dependencies
set(MCAP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/mcap/cpp/mcap/include)
target_include_directories(OSIUtilities PUBLIC ${MCAP_INCLUDE_DIR})

# Find compression libraries (LZ4, ZSTD)
include(FindCompression)
find_compression_libraries()

if (COMPRESSION_FOUND)
    target_link_libraries(OSIUtilities PRIVATE ${LZ4_TARGET} ${ZSTD_TARGET})
else ()
    message(FATAL_ERROR "Could not find LZ4 and ZSTD compression libraries")
endif ()
