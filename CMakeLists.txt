cmake_minimum_required(VERSION 3.16)

# An imported target missing its location property fails during generation.
if(POLICY CMP0111)
  cmake_policy(SET CMP0111 NEW)
endif()

project(mixxx VERSION 2.4.0)

add_library(mixxx-lib STATIC EXCLUDE_FROM_ALL
 src/util/fpclassify.cpp
)

find_package(Qt5Keychain REQUIRED)
target_link_libraries(mixxx-lib PRIVATE qt5keychain)

add_executable(mixxx WIN32 src/util/fpclassify.cpp)
target_link_libraries(mixxx PRIVATE mixxx-lib)
