# This file is part of Mixxx, Digital DJ'ing software.
# Copyright (C) 2001-2024 Mixxx Development Team
# Distributed under the GNU General Public Licence (GPL) version 2 or any later
# later version. See the LICENSE file for details.

#[=======================================================================[.rst:
FindAubio
--------

Finds the Aubio library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Aubio::Aubio``
  The Aubio library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Aubio_FOUND``
  True if the system has the Aubio library.
``Aubio_INCLUDE_DIRS``
  Include directories needed to use Aubio.
``Aubio_LIBRARIES``
  Libraries needed to link to Aubio.
``Aubio_DEFINITIONS``
  Compile definitions needed to use Aubio.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Aubio_INCLUDE_DIR``
  The directory containing ``aubio/aubio.h``.
``Aubio_LIBRARY``
  The path to the Aubio library.

#]=======================================================================]

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_Aubio QUIET aubio)
endif()

find_path(Aubio_INCLUDE_DIR
  NAMES aubio/aubio.h
  HINTS ${PC_Aubio_INCLUDE_DIRS}
  DOC "Aubio include directory")
mark_as_advanced(Aubio_INCLUDE_DIR)

find_library(Aubio_LIBRARY
  NAMES aubio
  HINTS ${PC_Aubio_LIBRARY_DIRS}
  DOC "Aubio library"
)
mark_as_advanced(Aubio_LIBRARY)

if(DEFINED PC_Aubio_VERSION AND NOT PC_Aubio_VERSION STREQUAL "")
  set(Aubio_VERSION "${PC_Aubio_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Aubio
  REQUIRED_VARS Aubio_LIBRARY Aubio_INCLUDE_DIR
  VERSION_VAR Aubio_VERSION
)

if(Aubio_FOUND)
  if(NOT TARGET Aubio::Aubio)
    add_library(Aubio::Aubio UNKNOWN IMPORTED)
    set_target_properties(Aubio::Aubio
      PROPERTIES
        IMPORTED_LOCATION "${Aubio_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PC_Aubio_CFLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${Aubio_INCLUDE_DIR}"
    )
  endif()
endif()
