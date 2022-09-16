cmake_minimum_required(VERSION 3.16)

# An imported target missing its location property fails during generation.
if(POLICY CMP0111)
  cmake_policy(SET CMP0111 NEW)
endif()

project(mixxx VERSION 2.4.0)

add_library(mixxx-lib STATIC EXCLUDE_FROM_ALL
  src/analyzer/analyzerbeats.cpp)

find_package(Qt5Keychain REQUIRED)
target_link_libraries(mixxx-lib PRIVATE qt5keychain)
