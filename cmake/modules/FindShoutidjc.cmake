#[=======================================================================[.rst:
FindShoutidjc
---------

Finds the Shoutidjc library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Shoutidjc::Shoutidjc``
  The Shout library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Shout_FOUND``
  True if the system has the Shoutidjc library.
``Shoutidjc_INCLUDE_DIRS``
  Include directories needed to use Shoutidjc.
``Shoutidjc_LIBRARIES``
  Libraries needed to link to Shoutidjc.
``Shoutidjc_DEFINITIONS``
  Compile definitions needed to use Shoutidjc.
``Shoutidjc_VERSION``
  The Version of libshout-idjc.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Shoutidjc_INCLUDE_DIR``
  The directory containing ``shoutidjc/shout.h``.
``Shoutidjc_LIBRARY``
  The path to the Shout library.

#]=======================================================================]

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_Shoutidjc QUIET shout-idjc)
endif()

find_path(Shoutidjc_INCLUDE_DIR
  NAMES shoutidjc/shout.h
  HINTS ${PC_Shout_INCLUDE_DIRS}
  DOC "Shout include directory")
mark_as_advanced(Shoutidjc_INCLUDE_DIR)

find_library(Shoutidjc_LIBRARY
  NAMES shout-idjc
  HINTS ${PC_Shoutidjc_LIBRARY_DIRS}
  DOC "Shoutidjc library"
)
mark_as_advanced(Shoutidjc_LIBRARY)

if(DEFINED PC_Shoutidjc_VERSION AND NOT PC_Shoutidjc_VERSION STREQUAL "")
  set(Shoutidjc_VERSION "${PC_Shoutidjc_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Shoutidjc
  REQUIRED_VARS Shoutidjc_LIBRARY Shoutidjc_INCLUDE_DIR
  VERSION_VAR Shoutidjc_VERSION
)

if(Shoutidjc_FOUND)
  set(Shoutidjc_LIBRARIES "${Shoutidjc_LIBRARY}")
  set(Shoutidjc_INCLUDE_DIRS "${Shoutidjc_INCLUDE_DIR}")
  set(Shoutidjc_DEFINITIONS ${PC_Shoutidjc_CFLAGS_OTHER})
  set(Shoutidjc_VERSION ${PC_Shoutidjc_VERSION})

  if(NOT TARGET Shoutidjc::Shoutidjc)
    add_library(Shoutidjc::Shoutidjc UNKNOWN IMPORTED)
    set_target_properties(Shoutidjc::Shoutidjc
      PROPERTIES
        IMPORTED_LOCATION "${Shoutidjc_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PC_Shoutidjc_CFLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${Shoutidjc_INCLUDE_DIR}"
    )
  endif()
endif()
