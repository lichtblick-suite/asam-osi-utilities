# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
# Add subdirectories for organized modules
include_directories(tracefile)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# specify library source files
set(OSIUtilities_SRCS
        tracefile/FilenameUtils.cpp
        tracefile/reader/Reader.cpp
        tracefile/MCAPImplementation.cpp
        tracefile/reader/SingleChannelBinaryTraceFileReader.cpp
        tracefile/writer/SingleChannelBinaryTraceFileWriter.cpp
        tracefile/reader/TXTHTraceFileReader.cpp
        tracefile/writer/TXTHTraceFileWriter.cpp
        tracefile/reader/MCAPTraceFileReader.cpp
        tracefile/writer/MCAPTraceFileWriter.cpp
        tracefile/writer/MCAPTraceFileChannel.cpp
)

# Create a library target for the entire library
add_library(OSIUtilities ${OSIUtilities_SRCS})
add_library(OSIUtilities::OSIUtilities ALIAS OSIUtilities)

target_compile_features(OSIUtilities PUBLIC cxx_std_17)
set_target_properties(OSIUtilities PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    EXPORT_NAME OSIUtilities
)

# add a compile definition to specify the version of the trace file specification.
# This is not the version of the linked OSI library, but the version of the trace file spec
# which is implemented (in functions, methods etc.) by this library (OSIUtilities).
target_compile_definitions(OSIUtilities PRIVATE OSI_TRACE_FILE_SPEC_VERSION="${OSI_TRACE_FILE_SPEC_VERSION}")


# Specify include paths for build and install usage
target_include_directories(OSIUtilities
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/cpp/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# MCAP is header-only with a single implementation TU. On Windows, its default
# visibility uses dllimport/dllexport which causes LNK4217/LNK4286 warnings
# when linking statically. Override MCAP_PUBLIC to empty for static builds.
if (WIN32)
    target_compile_definitions(OSIUtilities PUBLIC MCAP_PUBLIC=)
endif ()


# Link against OSI made available by parent CMakeLists.txt.
# PUBLIC because the public API exposes protobuf types (google::protobuf::Message
# in ReadResult, google::protobuf::Descriptor* in AddChannel) and consumers need
# OSI headers to instantiate WriteMessage<osi3::GroundTruth>() etc.
if (LINK_WITH_SHARED_OSI)
    set(OSI_LINK_TARGET open_simulation_interface::open_simulation_interface)
else ()
    set(OSI_LINK_TARGET open_simulation_interface::open_simulation_interface_pic)
endif ()
target_link_libraries(OSIUtilities PUBLIC ${OSI_LINK_TARGET})


# get mcap and its dependencies
set(MCAP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/mcap/cpp/mcap/include)
target_include_directories(OSIUtilities
    PUBLIC
        $<BUILD_INTERFACE:${MCAP_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Find compression libraries (LZ4, ZSTD)
include(FindCompression)
find_compression_libraries()

if (COMPRESSION_FOUND)
    # PRIVATE: lz4/zstd are not exposed in any public header — they are used
    # internally by the MCAP implementation compiled into OSIUtilities.
    # CMake still propagates them in the link line for static library consumers.
    target_link_libraries(OSIUtilities PRIVATE ${LZ4_TARGET} ${ZSTD_TARGET})
else ()
    message(FATAL_ERROR "Could not find LZ4 and ZSTD compression libraries")
endif ()

# --- Install rules ---

install(TARGETS OSIUtilities
    EXPORT OSIUtilitiesTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/cpp/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${MCAP_INCLUDE_DIR}/mcap DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# --- Export targets for find_package() ---

install(EXPORT OSIUtilitiesTargets
    NAMESPACE OSIUtilities::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OSIUtilities
)

# Generate and install package config file
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cpp/cmake/OSIUtilitiesConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OSIUtilitiesConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OSIUtilities
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OSIUtilitiesConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OSIUtilitiesConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OSIUtilitiesConfigVersion.cmake"
    # Install FindCompression so consumers can find lz4/zstd
    "${PROJECT_SOURCE_DIR}/cpp/cmake/FindCompression.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OSIUtilities
)

# Export from build tree for development without install
export(EXPORT OSIUtilitiesTargets
    NAMESPACE OSIUtilities::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/OSIUtilitiesTargets.cmake"
)
