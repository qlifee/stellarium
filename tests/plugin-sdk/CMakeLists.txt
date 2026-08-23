cmake_minimum_required(VERSION 3.18)

if(POLICY CMP0091)
	cmake_policy(SET CMP0091 NEW)
endif()

project(StellariumPluginSDKProbes LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
	message(FATAL_ERROR "The Stellarium plug-in SDK fixtures require Windows and MSVC.")
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

set(_expected_api_headers
	"core/StelApp.hpp"
	"core/StelModule.hpp"
	"core/StelPluginInterface.hpp")
set(_expected_dependency_headers
	"StelMainExport.hpp"
	"core/StelTextureTypes.hpp"
	"core/StelUtils.hpp"
	"core/VecMath.hpp")
set(_expected_headers
	${_expected_api_headers}
	${_expected_dependency_headers})
list(SORT _expected_headers)
if(NOT "${Stellarium_PLUGIN_SDK_API_HEADERS}" STREQUAL
		"${_expected_api_headers}" OR
	NOT "${Stellarium_PLUGIN_SDK_DEPENDENCY_HEADERS}" STREQUAL
		"${_expected_dependency_headers}" OR
	NOT "${Stellarium_PLUGIN_SDK_HEADERS}" STREQUAL
		"${_expected_headers}")
	message(FATAL_ERROR "The installed Stellarium SDK has an unexpected header surface.")
endif()

get_target_property(_target_definitions
	Stellarium::stelMain INTERFACE_COMPILE_DEFINITIONS)
set(_definition_probe_contents "")
foreach(_required_definition IN LISTS
		Stellarium_PLUGIN_SDK_REQUIRED_COMPILE_DEFINITIONS)
	if(NOT _required_definition IN_LIST _target_definitions)
		message(FATAL_ERROR
			"Stellarium::stelMain does not propagate ${_required_definition}.")
	endif()
	string(REGEX REPLACE "=.*$" "" _definition_name
		"${_required_definition}")
	string(APPEND _definition_probe_contents
		"#ifndef ${_definition_name}\n#error Missing ${_definition_name}\n#endif\n")
	if(_required_definition MATCHES "^_WIN32_WINNT=(.+)$")
		string(APPEND _definition_probe_contents
			"#if _WIN32_WINNT != ${CMAKE_MATCH_1}\n#error Wrong _WIN32_WINNT\n#endif\n")
	endif()
endforeach()
set(_definition_probe_source
	"${CMAKE_CURRENT_BINARY_DIR}/sdk-definitions.cpp")
file(WRITE "${_definition_probe_source}" "${_definition_probe_contents}")
add_library(StellariumSDKDefinitionProbe OBJECT
	"${_definition_probe_source}")
target_link_libraries(StellariumSDKDefinitionProbe
	PRIVATE Stellarium::stelMain)
set_target_properties(StellariumSDKDefinitionProbe PROPERTIES
	AUTOMOC OFF
	DISABLE_PRECOMPILE_HEADERS ON)

set(_header_probe_sources)
foreach(_header IN LISTS Stellarium_PLUGIN_SDK_HEADERS)
	string(MAKE_C_IDENTIFIER "${_header}" _header_id)
	set(_header_source
		"${CMAKE_CURRENT_BINARY_DIR}/sdk-header-${_header_id}.cpp")
	file(WRITE "${_header_source}" "#include <${_header}>\n")
	list(APPEND _header_probe_sources "${_header_source}")
endforeach()
if(NOT _header_probe_sources)
	message(FATAL_ERROR "The installed Stellarium SDK describes no headers.")
endif()

add_library(StellariumSDKHeaderProbe OBJECT ${_header_probe_sources})
target_link_libraries(StellariumSDKHeaderProbe PRIVATE Stellarium::stelMain)
set_target_properties(StellariumSDKHeaderProbe PROPERTIES
	AUTOMOC OFF
	DISABLE_PRECOMPILE_HEADERS ON)

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

add_library(StellariumStateProbe MODULE
	StellariumStateProbe.cpp
	StellariumStateProbe.hpp)
target_link_libraries(StellariumStateProbe PRIVATE Stellarium::stelMain)
target_compile_definitions(StellariumStateProbe PRIVATE
	STELLARIUM_EXPECTED_VERSION="${Stellarium_VERSION}")
set_target_properties(StellariumStateProbe PROPERTIES
	PREFIX "lib"
	OUTPUT_NAME "StellariumStateProbe"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

add_executable(StellariumPluginProbeLoader ProbeLoader.cpp)
target_link_libraries(StellariumPluginProbeLoader PRIVATE Stellarium::stelMain)
target_compile_definitions(StellariumPluginProbeLoader PRIVATE
	STELLARIUM_EXPECTED_VERSION="${Stellarium_VERSION}")
set_target_properties(StellariumPluginProbeLoader PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
