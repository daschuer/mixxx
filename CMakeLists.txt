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

project(mixxx VERSION 2.4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(CMakeDependentOption)
include(CheckSymbolExists)
include(CheckIncludeFileCXX)
include(ExternalProject)
include(GNUInstallDirs)
include(DefaultOption)
include(IsStaticLibrary)

set(CMAKE_CXX_STANDARD 20)

add_library(mixxx-lib STATIC EXCLUDE_FROM_ALL
 src/util/fpclassify.cpp
)

set(CMAKE_FIND_DEBUG_MODE TRUE)
find_package(Qt5Keychain REQUIRED)
set(CMAKE_FIND_DEBUG_MODE FALSE)
message(STATUS "Qt5Keychain found")
target_link_libraries(mixxx-lib PRIVATE qt5keychain)
message(STATUS "Qt5Keychain added")

add_executable(mixxx WIN32 src/util/fpclassify.cpp)
target_link_libraries(mixxx PRIVATE mixxx-lib)
