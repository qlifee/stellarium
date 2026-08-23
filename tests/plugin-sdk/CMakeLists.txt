cmake_minimum_required(VERSION 3.18)

if(POLICY CMP0091)
	cmake_policy(SET CMP0091 NEW)
endif()

project(DynamicPluginProbe LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
	message(FATAL_ERROR "The DynamicPluginProbe fixture requires Windows and MSVC.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING
		"Configurations supported by the Stellarium plug-in ABI probe" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
	message(FATAL_ERROR "The Stellarium plug-in ABI probe must be built as Release.")
endif()

find_package(Stellarium 26.2.0 EXACT CONFIG REQUIRED)

if(NOT Stellarium_PLUGIN_ABI_BUILD_CONFIGURATION STREQUAL "Release")
	message(FATAL_ERROR "The installed Stellarium SDK does not describe a Release build.")
endif()

add_library(DynamicPluginProbe MODULE
	DynamicPluginProbe.cpp
	DynamicPluginProbe.hpp)
target_link_libraries(DynamicPluginProbe PRIVATE Stellarium::stelMain)
set_target_properties(DynamicPluginProbe PROPERTIES
	PREFIX "lib"
	OUTPUT_NAME "DynamicPluginProbe"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

add_executable(DynamicPluginProbeLoader ProbeLoader.cpp)
target_link_libraries(DynamicPluginProbeLoader PRIVATE Stellarium::stelMain)
target_compile_definitions(DynamicPluginProbeLoader PRIVATE
	STELLARIUM_EXPECTED_VERSION="${Stellarium_VERSION}")
set_target_properties(DynamicPluginProbeLoader PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
