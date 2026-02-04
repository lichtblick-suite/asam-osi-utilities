#
# Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#

# VersionFromFile.cmake
# Reads the version from the VERSION file and sets project version variables
#
# This module sets the following variables:
#   PROJECT_VERSION_FROM_FILE - Full version string (e.g., "0.1.0")
#   PROJECT_VERSION_MAJOR_FROM_FILE - Major version number
#   PROJECT_VERSION_MINOR_FROM_FILE - Minor version number  
#   PROJECT_VERSION_PATCH_FROM_FILE - Patch version number

function(read_version_from_file)
    set(VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
    
    if(NOT EXISTS "${VERSION_FILE}")
        message(FATAL_ERROR "VERSION file not found at ${VERSION_FILE}")
    endif()
    
    file(READ "${VERSION_FILE}" VERSION_CONTENT)
    string(STRIP "${VERSION_CONTENT}" VERSION_STRING)
    
    # Parse version components
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${VERSION_STRING}")
    
    if(NOT VERSION_MATCH)
        message(FATAL_ERROR "Invalid version format in VERSION file: ${VERSION_STRING}. Expected format: MAJOR.MINOR.PATCH")
    endif()
    
    set(VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(VERSION_MINOR "${CMAKE_MATCH_2}")
    set(VERSION_PATCH "${CMAKE_MATCH_3}")
    
    # Export to parent scope
    set(PROJECT_VERSION_FROM_FILE "${VERSION_STRING}" PARENT_SCOPE)
    set(PROJECT_VERSION_MAJOR_FROM_FILE "${VERSION_MAJOR}" PARENT_SCOPE)
    set(PROJECT_VERSION_MINOR_FROM_FILE "${VERSION_MINOR}" PARENT_SCOPE)
    set(PROJECT_VERSION_PATCH_FROM_FILE "${VERSION_PATCH}" PARENT_SCOPE)
    
    message(STATUS "Project version from VERSION file: ${VERSION_STRING}")
endfunction()
