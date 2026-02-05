# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#
# FindCompression.cmake
# Finds LZ4 and ZSTD compression libraries with cross-platform support
#
# This module sets the following variables:
#   COMPRESSION_FOUND - True if both LZ4 and ZSTD were found
#   LZ4_TARGET - The CMake target for LZ4
#   ZSTD_TARGET - The CMake target for ZSTD
#
# Usage:
#   include(FindCompression)
#   find_compression_libraries()
#   target_link_libraries(mytarget PRIVATE ${LZ4_TARGET} ${ZSTD_TARGET})

function(find_compression_libraries)
    # Try CONFIG mode first (vcpkg, conan, etc.)
    find_package(LZ4 CONFIG QUIET)
    find_package(lz4 CONFIG QUIET)
    find_package(ZSTD CONFIG QUIET)
    find_package(zstd CONFIG QUIET)

    # Determine LZ4 target
    set(LZ4_TARGET_LOCAL "")
    if (TARGET LZ4::lz4)
        set(LZ4_TARGET_LOCAL LZ4::lz4)
    elseif (TARGET lz4::lz4)
        set(LZ4_TARGET_LOCAL lz4::lz4)
    endif ()

    # Determine ZSTD target
    set(ZSTD_TARGET_LOCAL "")
    if (TARGET ZSTD::ZSTD)
        set(ZSTD_TARGET_LOCAL ZSTD::ZSTD)
    elseif (TARGET zstd::libzstd_static)
        set(ZSTD_TARGET_LOCAL zstd::libzstd_static)
    elseif (TARGET zstd::libzstd_shared)
        set(ZSTD_TARGET_LOCAL zstd::libzstd_shared)
    endif ()

    # Fallback to pkg-config if CONFIG mode didn't find targets
    if (NOT LZ4_TARGET_LOCAL OR NOT ZSTD_TARGET_LOCAL)
        find_package(PkgConfig REQUIRED)
        
        if (NOT LZ4_TARGET_LOCAL)
            pkg_check_modules(lz4 REQUIRED IMPORTED_TARGET liblz4)
            set(LZ4_TARGET_LOCAL PkgConfig::lz4)
        endif ()
        
        if (NOT ZSTD_TARGET_LOCAL)
            pkg_check_modules(zstd REQUIRED IMPORTED_TARGET libzstd)
            set(ZSTD_TARGET_LOCAL PkgConfig::zstd)
        endif ()
    endif ()

    # Export to parent scope
    set(LZ4_TARGET ${LZ4_TARGET_LOCAL} PARENT_SCOPE)
    set(ZSTD_TARGET ${ZSTD_TARGET_LOCAL} PARENT_SCOPE)
    
    if (LZ4_TARGET_LOCAL AND ZSTD_TARGET_LOCAL)
        set(COMPRESSION_FOUND TRUE PARENT_SCOPE)
    else ()
        set(COMPRESSION_FOUND FALSE PARENT_SCOPE)
    endif ()
endfunction()
