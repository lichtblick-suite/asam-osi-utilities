cmake_minimum_required(VERSION 3.16)

# Add cmake modules directory early so we can use custom find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Read version from vcpkg.json (single source of truth)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" VCPKG_JSON)
string(REGEX MATCH "\"version-string\"[ \t]*:[ \t]*\"([^\"]+)\"" _ "${VCPKG_JSON}")
if (NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Unable to read version-string from vcpkg.json")
endif ()
set(PROJECT_VERSION_FROM_VCPKG "${CMAKE_MATCH_1}")

project(asam-osi-utilities VERSION ${PROJECT_VERSION_FROM_VCPKG})

# Generate compile_commands.json for clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

# Add the include folder for public headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Build options
option(BUILD_DOCS "Build Doxygen documentation" OFF)
option(BUILD_TESTING "Build unit tests" OFF)
option(BUILD_EXAMPLES "Build example programs" ON)
option(OSIUTILITIES_DOCS_ONLY "Build only documentation (skip library/examples/tests)" OFF)
option(OSIUTILITIES_RUN_TESTS "Run tests after build (implies BUILD_TESTING=ON)" OFF)
option(LINK_WITH_SHARED_OSI "Link utils with shared OSI library instead of statically linking" OFF)

if (OSIUTILITIES_DOCS_ONLY)
    set(BUILD_DOCS ON CACHE BOOL "Build Doxygen documentation" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Build unit tests" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Build example programs" FORCE)
    if (OSIUTILITIES_RUN_TESTS)
        message(WARNING "OSIUTILITIES_RUN_TESTS is ignored when OSIUTILITIES_DOCS_ONLY=ON")
        set(OSIUTILITIES_RUN_TESTS OFF CACHE BOOL "Run tests after build (implies BUILD_TESTING=ON)" FORCE)
    endif ()
endif ()

if (OSIUTILITIES_RUN_TESTS)
    set(BUILD_TESTING ON CACHE BOOL "Build unit tests" FORCE)
endif ()

if (DEFINED USE_EXTERNAL_OSI AND USE_EXTERNAL_OSI)
    message(FATAL_ERROR "External OSI is disabled; use the bundled lib/osi-cpp submodule.")
elseif (DEFINED USE_EXTERNAL_OSI)
    message(STATUS "Ignoring legacy USE_EXTERNAL_OSI option; using bundled submodule.")
endif ()

if (NOT OSIUTILITIES_DOCS_ONLY)
    # Use bundled osi-cpp submodule, which builds open_simulation_interface.
    set(OSI_CPP_DIR ${PROJECT_SOURCE_DIR}/lib/osi-cpp)
    if (NOT EXISTS "${OSI_CPP_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "osi-cpp submodule not found. Run: git submodule update --init --recursive")
    endif ()
    message(STATUS "Using bundled osi-cpp submodule at ${OSI_CPP_DIR}")
    add_subdirectory(${OSI_CPP_DIR} ${CMAKE_BINARY_DIR}/osi-cpp EXCLUDE_FROM_ALL)
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
if (BUILD_DOCS AND PROJECT_IS_TOP_LEVEL)
    find_package(Doxygen REQUIRED)
    message(STATUS "Doxygen found, generating API documentation")
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile)
    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    add_custom_target(
            library_api_doc
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
    )
    if (OSIUTILITIES_DOCS_ONLY)
        add_custom_target(docs_only ALL DEPENDS library_api_doc)
    endif ()
endif ()

if (NOT OSIUTILITIES_DOCS_ONLY)
    add_subdirectory(src)
    if (PROJECT_IS_TOP_LEVEL)
        include(CTest)
        if (BUILD_TESTING)
            enable_testing()
            add_subdirectory(tests)
            if (OSIUTILITIES_RUN_TESTS)
                set(CTEST_CONFIG_ARG "")
                if (CMAKE_CONFIGURATION_TYPES)
                    set(CTEST_CONFIG_ARG "-C" "$<CONFIG>")
                endif ()
                add_custom_target(
                        run_tests
                        ALL
                        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure ${CTEST_CONFIG_ARG}
                        DEPENDS unit_tests
                        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                        USES_TERMINAL
                )
            endif ()
        endif ()
    endif ()
    if (BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif ()
endif ()
