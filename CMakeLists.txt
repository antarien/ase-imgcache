cmake_minimum_required(VERSION 3.22)

#[[
ASE IMGCACHE BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-imgcache
@description LRU image cache backed by Gdk::Pixbuf and Cairo surfaces.
             Single source of truth for image loading in all native ASE
             clients. Pre-scales images per (path, max_width) key so repeated
             renders never re-decode or re-scale.

@module      ase-imgcache
@layer       Shared (L1, consumed by L5 native clients)
@created     2026-04-14
@modified    2026-04-14
@version     00.00.01.00001 [seed]

CMAKE MODULE COMPLIANCE

[x] cmake_minimum_required(VERSION 3.22) as FIRST line
[x] project() with VERSION and LANGUAGES CXX
[x] message(STATUS) for configuration output
[x] add_library() STATIC (has .cpp)
[x] add_library(ase::{module} ALIAS ...) for namespace
[x] target_include_directories() with PUBLIC and generator expressions
[x] target_compile_features() with cxx_std_20
[x] include(GNUInstallDirs) for installation
[x] install(DIRECTORY include/ ...) for headers
]]

# Parse local VERSION (SSOT) if not building from root
if(NOT DEFINED ASE_IMGCACHE_VERSION)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        message(FATAL_ERROR "VERSION file not found: ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _VERSION_LINES)
    foreach(_LINE ${_VERSION_LINES})
        if(_LINE MATCHES "^ASE_IMGCACHE=(.*)$")
            set(ASE_IMGCACHE_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED ASE_IMGCACHE_VERSION)
        message(FATAL_ERROR "ASE_IMGCACHE version not found in VERSION file")
    endif()
endif()

project(ase-imgcache VERSION ${ASE_IMGCACHE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

# ──────────────────────────────────────────────────────────────────────
# System dependencies
# ──────────────────────────────────────────────────────────────────────
# Cairomm + GdkPixbuf come transitively through ase::adp::gtk (adapter layer).
# If ase::adp::gtk is not available (e.g. standalone subproject build), fall back
# to pkg-config directly against gtkmm-4.0.
if(NOT TARGET ase::adp::gtk)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
endif()

# ase::utils provides the ase-utils::strops string helpers (str_copy,
# str_append, str_equal) that replace the forbidden C string functions.
if(NOT TARGET ase::utils)
    message(FATAL_ERROR
        "ase-imgcache requires ase::utils. When building standalone, add "
        "foundation/ase-utils via add_subdirectory() before this module.")
endif()

# ──────────────────────────────────────────────────────────────────────
# Library
# ──────────────────────────────────────────────────────────────────────
add_library(ase-imgcache STATIC
    src/image_cache.cpp
)
add_library(ase::imgcache ALIAS ase-imgcache)

target_include_directories(ase-imgcache
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ase-imgcache PUBLIC cxx_std_20)

target_link_libraries(ase-imgcache PUBLIC ase::utils)

if(TARGET ase::adp::gtk)
    target_link_libraries(ase-imgcache PUBLIC ase::adp::gtk)
else()
    target_link_libraries(ase-imgcache PUBLIC PkgConfig::GTKMM)
endif()

# ──────────────────────────────────────────────────────────────────────
# Installation
# ──────────────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS ase-imgcache
    EXPORT ase-imgcache-targets
)

install(DIRECTORY include/ase
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
