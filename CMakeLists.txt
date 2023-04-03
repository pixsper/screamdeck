cmake_minimum_required(VERSION 3.16.3)
project(screamdeck VERSION 0.1.0 DESCRIPTION "C library for fast output to Elgato Streamdeck display")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                EXPECTED_HASH SHA256=5cdb3042632da3efff558924eecefd580a0e786863a857ca097c3d1d43df5dcd
                TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(REQUIRES hidapi/0.13.1
							   libjpeg-turbo/2.1.5
							   xxhash/0.8.1
					  IMPORTS "bin, *.dll -> ./"
					  GENERATORS cmake_find_package
					  OPTIONS )

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${settings})

find_package(hidapi)
find_package(JPEG)
find_package(xxHash)

add_library(screamdeck SHARED)

set_target_properties(screamdeck PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(screamdeck PROPERTIES SOVERSION 1)
set_target_properties(screamdeck PROPERTIES PUBLIC_HEADER include/screamdeck.h)

target_include_directories(screamdeck PUBLIC
	"include"
)

target_sources(screamdeck PRIVATE
	"src/screamdeck.c"
	"example/screamdeck_example.c")

target_link_libraries(screamdeck PRIVATE JPEG::turbojpeg-static xxHash::xxhash)

if(UNIX AND NOT APPLE)
	target_link_libraries(screamdeck PRIVATE hidapi::hidraw)
else()
	target_link_libraries(screamdeck PRIVATE hidapi::hidapi)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_link_options(screamdeck PRIVATE "-Wl,--exclude-libs,ALL")
endif()

add_executable(screamdeck_example)

target_link_libraries(screamdeck_example PUBLIC JPEG::turbojpeg-static screamdeck)

if(UNIX AND NOT APPLE)
	target_link_libraries(screamdeck_example PRIVATE udev)
endif()

target_sources(screamdeck_example PRIVATE
	"example/screamdeck_example.c")