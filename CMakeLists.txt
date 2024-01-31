cmake_minimum_required(VERSION 3.24)
project(screamdeck LANGUAGES C VERSION 0.2.0 DESCRIPTION "C library for fast output to Elgato Streamdeck display")

# Annoyingly the conan hidapi recipe doesn't currently support static builds for Windows, so we include via git submodule
set(BUILD_SHARED_LIBS FALSE)
add_subdirectory(lib/hidapi)

find_package(libjpeg-turbo REQUIRED)
find_package(xxHash REQUIRED)

add_library(${PROJECT_NAME} SHARED)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION 1)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER include/screamdeck.h)

target_include_directories(${PROJECT_NAME} PUBLIC
	"include"
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"lib/hidapi"
)

target_sources(${PROJECT_NAME} PRIVATE
	"src/screamdeck.c")

target_link_libraries(${PROJECT_NAME} PRIVATE libjpeg-turbo::turbojpeg-static xxHash::xxhash)

if(UNIX AND NOT APPLE)
	target_link_libraries(${PROJECT_NAME} PRIVATE hidapi::hidraw)
else()
	target_link_libraries(${PROJECT_NAME} PRIVATE hidapi::hidapi)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--exclude-libs,ALL")
endif()

add_executable(${PROJECT_NAME}_example)

target_link_libraries(${PROJECT_NAME}_example PUBLIC libjpeg-turbo::turbojpeg-static screamdeck)

if(UNIX AND NOT APPLE)
	target_link_libraries(${PROJECT_NAME}_example PRIVATE udev)
endif()

target_sources(${PROJECT_NAME}_example PRIVATE
	"example/screamdeck_example.c")