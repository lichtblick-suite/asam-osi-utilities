cmake_minimum_required(VERSION 3.16)

# Add cmake modules directory early so we can use VersionFromFile
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Read version from VERSION file (single source of truth)
include(VersionFromFile)
read_version_from_file()

project(asam-osi-utilities VERSION ${PROJECT_VERSION_FROM_FILE})

# Some features (e.g. mcap) require at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Generate compile_commands.json for clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

# Add the include folder for public headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# option for specifying the path to the open_simulation_interface.
option(USE_EXTERNAL_OSI "Use an external version of open_simulation_interface" OFF)
option(LINK_WITH_SHARED_OSI "Link utils with shared OSI library instead of statically linking" OFF)

# Locate open_simulation_interface
if (USE_EXTERNAL_OSI)
    # If you use this library in your own project, ensure CMake can locate
    # the open_simulation_interface package (e.g. via CMAKE_PREFIX_PATH or open_simulation_interface_DIR).
    find_package(open_simulation_interface QUIET REQUIRED)
    message(STATUS "Using external OSI, provided by ${open_simulation_interface_DIR}")
    if (DEFINED open_simulation_interface_VERSION)
        set(OSI_VERSION "${open_simulation_interface_VERSION}")
    elseif (DEFINED OPEN_SIMULATION_INTERFACE_VERSION)
        set(OSI_VERSION "${OPEN_SIMULATION_INTERFACE_VERSION}")
    endif ()
else ()
    # Use bundled osi-cpp submodule, which builds open_simulation_interface.
    set(OSI_CPP_DIR ${PROJECT_SOURCE_DIR}/lib/osi-cpp)
    if (NOT EXISTS "${OSI_CPP_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "osi-cpp submodule not found. Run: git submodule update --init --recursive")
    endif ()
    message(STATUS "Using bundled osi-cpp submodule at ${OSI_CPP_DIR}")
    add_subdirectory(${OSI_CPP_DIR} ${CMAKE_BINARY_DIR}/osi-cpp)
    get_directory_property(OSI_VERSION_MAJOR DIRECTORY ${OSI_CPP_DIR} DEFINITION VERSION_MAJOR)
    get_directory_property(OSI_VERSION_MINOR DIRECTORY ${OSI_CPP_DIR} DEFINITION VERSION_MINOR)
    get_directory_property(OSI_VERSION_PATCH DIRECTORY ${OSI_CPP_DIR} DEFINITION VERSION_PATCH)
    if (OSI_VERSION_MAJOR AND OSI_VERSION_MINOR AND OSI_VERSION_PATCH)
        set(OSI_VERSION "${OSI_VERSION_MAJOR}.${OSI_VERSION_MINOR}.${OSI_VERSION_PATCH}")
    endif ()
endif ()

# Trace file spec version implemented by this library (default matches OSI 3.8.0-rc1 tag used in submodule)
set(OSI_TRACE_FILE_SPEC_VERSION "3.8.0-rc1" CACHE STRING "Trace file specification version implemented by OSIUtilities")
if (OSI_VERSION AND NOT OSI_VERSION STREQUAL OSI_TRACE_FILE_SPEC_VERSION)
    message(STATUS "OSI version detected: ${OSI_VERSION} (trace file spec: ${OSI_TRACE_FILE_SPEC_VERSION})")
endif ()


# option to enable coverage reporting
option(CODE_COVERAGE "Enable coverage reporting" OFF)
if (CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
    message(WARNING "Added coverage options")
endif ()

# build doxygen documentation
option(BUILD_DOCS "Build Doxygen documentation" ON)
find_package(Doxygen)
if (DOXYGEN_FOUND AND PROJECT_IS_TOP_LEVEL AND BUILD_DOCS)
    message(STATUS "Doxygen found, generating API documentation")
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile)
    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    # note the option ALL which allows to build the docs together with the application
    add_custom_target(
            library_api_doc
            ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
    )
endif ()

add_subdirectory(src)
if (PROJECT_IS_TOP_LEVEL)
    include(CTest)
    if (BUILD_TESTING)
        enable_testing()
        add_subdirectory(tests)
    endif ()
endif ()
add_subdirectory(examples)
