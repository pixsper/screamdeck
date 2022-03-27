cmake_minimum_required(VERSION 3.19)
project(screamdeck)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.17.0/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                EXPECTED_HASH SHA256=3bef79da16c2e031dc429e1dac87a08b9226418b300ce004cc125a82687baeef
                TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(REQUIRES hidapi/0.11.2
							   libjpeg-turbo/2.1.2
					  IMPORTS "bin, *.dll -> ./"
					  GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${settings})
					
find_package(hidapi)
find_package(libjpeg-turbo)

add_library(screamdeck SHARED)

target_include_directories(screamdeck PUBLIC
	"include"
)

target_sources(screamdeck PRIVATE
	"src/screamdeck.c"
 "example/screamdeck_example.c")

target_link_libraries(screamdeck PRIVATE hidapi::hidapi libjpeg-turbo::turbojpeg-static)


 add_executable(screamdeck_example)

 target_link_libraries(screamdeck_example PUBLIC screamdeck)

 target_sources(screamdeck_example PRIVATE
	"example/screamdeck_example.c")

