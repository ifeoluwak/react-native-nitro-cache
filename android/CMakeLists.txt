project(nitrocache)
cmake_minimum_required(VERSION 3.9.0)

set(PACKAGE_NAME nitrocache)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 20)

# Define C++ library and add all sources
add_library(${PACKAGE_NAME} SHARED
        src/main/cpp/cpp-adapter.cpp
        ../cpp/NitroCache.cpp)

# Add Nitrogen specs :)
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/nitrocache+autolinking.cmake)

# Set up local includes
include_directories("src/main/cpp" "../cpp")

# Headers for C++ integration with react-native-nitro-fetch (package or hoisted node_modules)
foreach(
  _nitro_fetch_include
  "${CMAKE_SOURCE_DIR}/../node_modules/react-native-nitro-fetch/nitrogen/generated/shared/c++"
  "${CMAKE_SOURCE_DIR}/../../node_modules/react-native-nitro-fetch/nitrogen/generated/shared/c++"
)
  if(EXISTS "${_nitro_fetch_include}")
    include_directories("${_nitro_fetch_include}")
    break()
  endif()
endforeach()


find_library(LOG_LIB log)

# Link all libraries together
target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB}
        android # <-- Android core
)
