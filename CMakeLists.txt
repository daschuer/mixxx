cmake_minimum_required(VERSION 3.16)

# CMAKE_CXX_COMPILER_ID: Distinguish between "AppleClang" and "Clang"
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

# MACOSX_RPATH is set by default
if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

# Support new IN_LIST if() operator
if(POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

# Enforce interprocedural optimization
if(POLICY CMP0069)
  cmake_policy(SET CMP0069 NEW)
endif()

# Let AUTOMOC and AUTOUIC process GENERATED files
if(POLICY CMP0071)
  cmake_policy(SET CMP0071 NEW)
endif()

# Propagate interface link properties
if(POLICY CMP0099)
  # This avoids a warning when qt deals with different behaviours controlled by this policy
  # in its cmake functions. See
  # https://github.com/qt/qtbase/commit/e3e1007f9778d8fc629a06f7d3d216bb7f81351b
  cmake_policy(SET CMP0099 NEW)
endif()

# An imported target missing its location property fails during generation.
if(POLICY CMP0111)
  cmake_policy(SET CMP0111 NEW)
endif()

# Set the timestamp of extracted files to the time of the extraction instead of
# the archived timestamp to make sure that dependent files are rebuilt if the
# URL changes.
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Check if any relevant env vars were set from the build env scripts
if(DEFINED ENV{X_VCPKG_APPLOCAL_DEPS_INSTALL} AND NOT DEFINED X_VCPKG_APPLOCAL_DEPS_INSTALL)
  set(X_VCPKG_APPLOCAL_DEPS_INSTALL "$ENV{X_VCPKG_APPLOCAL_DEPS_INSTALL}" CACHE BOOL "")
endif()

# Set the timestamp of extracted files to the time of the extraction instead of
# the archived timestamp to make sure that dependent files are rebuilt if the
# URL changes.
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Set up vcpkg
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED VCPKG_ROOT)
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
endif()

if(DEFINED VCPKG_ROOT)
  set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON CACHE BOOL "Automatically copy dependencies into the install target directory for executables." FORCE)

  if(NOT DEFINED VCPKG_OVERLAY_PORTS)
    set(VCPKG_OVERLAY_PORTS "${VCPKG_ROOT}/overlay/ports")
    if(APPLE)
        list(APPEND VCPKG_OVERLAY_PORTS "${VCPKG_ROOT}/overlay/osx")
    elseif(WIN32)
        list(APPEND VCPKG_OVERLAY_PORTS "${VCPKG_ROOT}/overlay/windows")
    endif()
  endif()

  if(NOT DEFINED VCPKG_OVERLAY_TRIPLETS)
    set(VCPKG_OVERLAY_TRIPLETS "${VCPKG_ROOT}/overlay/triplets")
  endif()

  if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
    set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}")
  endif()

  if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
  endif()
endif()

# Set a default build type if none was specified
# See https://blog.kitware.com/cmake-and-the-default-build-type/ for details.
set(default_build_type "RelWithDebInfo")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND NOT WIN32)
  # On Windows, Debug builds are linked to unoptimized libs
  # generating unusable slow Mixxx builds.
  set(default_build_type "Debug")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
  if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting CMAKE_BUILD_TYPE to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
  elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release|RelWithDebInfo)$")
    message(FATAL_ERROR "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} is not supported, use one of Debug, Release or RelWithDebInfo.")
  endif()
endif()

option(QT6 "Build with Qt6" OFF)

if(APPLE)
  if(QT6)
    # Minimum macOS version supported by Qt 6
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Minimum macOS version the build will be able to run on")
    if(NOT VCPKG_TARGET_TRIPLET)
      set(VCPKG_TARGET_TRIPLET "x64-osx-min10.15")
    endif()
  else()
    # Minimum macOS version supported by Qt 5.12
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Minimum macOS version the build will be able to run on")
    if(NOT VCPKG_TARGET_TRIPLET)
      set(VCPKG_TARGET_TRIPLET "x64-osx-min1012")
    endif()
    # Needed for deployment target < 10.14
    add_compile_options(-fno-aligned-allocation)
  endif()
endif()

if(DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
  if(NOT EXISTS "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
    message(FATAL_ERROR "VCPKG_TARGET_TRIPLET dir not found: ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET} "
        "Make sure the VCPKG build environment is installed and contains the build for the selected triplet.")
  endif()
endif()

project(mixxx VERSION 2.4.0)
# Work around missing version suffixes support https://gitlab.kitware.com/cmake/cmake/-/issues/16716
set(MIXXX_VERSION_PRERELEASE "alpha-pre") # set to "alpha-pre" "beta" or ""

set(CMAKE_PROJECT_HOMEPAGE_URL "https://www.mixxx.org")
set(CMAKE_PROJECT_DESCRIPTION "Mixxx is Free DJ software that gives you everything you need to perform live mixes.")

# Used for force control of color output
set(BUILD_COLORS "auto" CACHE STRING "Try to use colors auto/always/no")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(CMakeDependentOption)
include(CheckSymbolExists)
include(CheckIncludeFileCXX)
include(ExternalProject)
include(GNUInstallDirs)
include(DefaultOption)
include(IsStaticLibrary)

add_library(mixxx-lib STATIC EXCLUDE_FROM_ALL
 src/util/fpclassify.cpp
)

find_package(Qt5Keychain REQUIRED)
target_link_libraries(mixxx-lib PRIVATE qt5keychain)

add_executable(mixxx WIN32 src/util/fpclassify.cpp)
target_link_libraries(mixxx PRIVATE mixxx-lib)
