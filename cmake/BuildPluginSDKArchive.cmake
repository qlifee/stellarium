CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

FOREACH(_required_variable
     PLUGIN_SDK_BUILD_DIR
     PLUGIN_SDK_STAGE_PARENT
     PLUGIN_SDK_ARCHIVE_DIR
     PLUGIN_SDK_BASENAME
     PLUGIN_SDK_CONFIGURATION
     PLUGIN_SDK_COMPONENT
     PLUGIN_SDK_SOURCE_DIR
     PLUGIN_SDK_SOURCE_REVISION
     PLUGIN_SDK_SOURCE_TREE_STATE
     PLUGIN_SDK_PROVENANCE_STATUS)
     IF(NOT DEFINED ${_required_variable} OR
          "${${_required_variable}}" STREQUAL "")
          MESSAGE(FATAL_ERROR
               "BuildPluginSDKArchive.cmake requires ${_required_variable}")
     ENDIF()
ENDFOREACH()

IF(NOT PLUGIN_SDK_CONFIGURATION STREQUAL "Release")
     MESSAGE(FATAL_ERROR
          "Build stellarium-plugin-sdk-archive with --config Release")
ENDIF()

IF(NOT PLUGIN_SDK_BASENAME MATCHES
     "^[A-Za-z0-9][A-Za-z0-9._+-]*$")
     MESSAGE(FATAL_ERROR
          "PLUGIN_SDK_BASENAME contains an unsupported character")
ENDIF()
STRING(FIND "${PLUGIN_SDK_BASENAME}" ".." _parent_reference)
IF(PLUGIN_SDK_BASENAME STREQUAL "." OR
     NOT _parent_reference EQUAL -1)
     MESSAGE(FATAL_ERROR
          "PLUGIN_SDK_BASENAME must not be '.' or contain '..'")
ENDIF()

IF(NOT PLUGIN_SDK_SOURCE_TREE_STATE MATCHES
     "^(clean|dirty|unknown)$")
     MESSAGE(FATAL_ERROR
          "PLUGIN_SDK_SOURCE_TREE_STATE must be clean, dirty, or unknown")
ENDIF()
IF(NOT PLUGIN_SDK_PROVENANCE_STATUS MATCHES
     "^(verified-clean|unverified)$")
     MESSAGE(FATAL_ERROR
          "PLUGIN_SDK_PROVENANCE_STATUS must be verified-clean or unverified")
ENDIF()
IF(PLUGIN_SDK_PROVENANCE_STATUS STREQUAL "verified-clean")
     IF(NOT PLUGIN_SDK_SOURCE_TREE_STATE STREQUAL "clean" OR
          PLUGIN_SDK_SOURCE_REVISION STREQUAL "unknown")
          MESSAGE(FATAL_ERROR
               "Verified SDK provenance requires a clean, known source revision")
     ENDIF()
ELSEIF(NOT PLUGIN_SDK_BASENAME MATCHES "-unverified$")
     MESSAGE(FATAL_ERROR
          "An unverified SDK archive must use an -unverified filename")
ENDIF()

GET_FILENAME_COMPONENT(_build_dir "${PLUGIN_SDK_BUILD_DIR}" ABSOLUTE)
GET_FILENAME_COMPONENT(_stage_parent "${PLUGIN_SDK_STAGE_PARENT}" ABSOLUTE)
GET_FILENAME_COMPONENT(_archive_dir "${PLUGIN_SDK_ARCHIVE_DIR}" ABSOLUTE)
GET_FILENAME_COMPONENT(_source_dir "${PLUGIN_SDK_SOURCE_DIR}" ABSOLUTE)
FILE(TO_CMAKE_PATH "${_build_dir}" _build_dir)
FILE(TO_CMAKE_PATH "${_stage_parent}" _stage_parent)
FILE(TO_CMAKE_PATH "${_archive_dir}" _archive_dir)
FILE(TO_CMAKE_PATH "${_source_dir}" _source_dir)

IF(NOT IS_DIRECTORY "${_source_dir}")
     MESSAGE(FATAL_ERROR
          "PLUGIN_SDK_SOURCE_DIR is not a directory: ${_source_dir}")
ENDIF()

SET(_build_prefix "${_build_dir}/")
STRING(FIND "${_stage_parent}/" "${_build_prefix}" _stage_prefix)
STRING(FIND "${_archive_dir}/" "${_build_prefix}" _archive_prefix)
IF(_stage_parent STREQUAL _build_dir OR NOT _stage_prefix EQUAL 0)
     MESSAGE(FATAL_ERROR
          "PLUGIN_SDK_STAGE_PARENT must be below PLUGIN_SDK_BUILD_DIR")
ENDIF()
IF(_archive_dir STREQUAL _build_dir OR NOT _archive_prefix EQUAL 0)
     MESSAGE(FATAL_ERROR
          "PLUGIN_SDK_ARCHIVE_DIR must be below PLUGIN_SDK_BUILD_DIR")
ENDIF()

# Git-backed packages must remain at the configured revision. A package which
# claimed a clean tree must also still be clean when the archive is created.
IF(NOT PLUGIN_SDK_SOURCE_TREE_STATE STREQUAL "unknown")
     IF(NOT DEFINED PLUGIN_SDK_GIT_EXECUTABLE OR
          NOT EXISTS "${PLUGIN_SDK_GIT_EXECUTABLE}")
          MESSAGE(FATAL_ERROR
               "A Git-backed SDK package requires the configured Git executable")
     ENDIF()
     EXECUTE_PROCESS(
          COMMAND "${PLUGIN_SDK_GIT_EXECUTABLE}" rev-parse HEAD
          WORKING_DIRECTORY "${_source_dir}"
          RESULT_VARIABLE _git_head_result
          OUTPUT_VARIABLE _git_head
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET)
     IF(NOT _git_head_result EQUAL 0 OR
          NOT "${_git_head}" STREQUAL "${PLUGIN_SDK_SOURCE_REVISION}")
          MESSAGE(FATAL_ERROR
               "The source revision changed after SDK configuration")
     ENDIF()

     EXECUTE_PROCESS(
          COMMAND "${PLUGIN_SDK_GIT_EXECUTABLE}" status --porcelain
               --untracked-files=no -- .
          WORKING_DIRECTORY "${_source_dir}"
          RESULT_VARIABLE _git_tracked_result
          OUTPUT_VARIABLE _git_tracked
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET)
     SET(_git_untracked_arguments
          ls-files --others --exclude-standard -- .)
     FILE(RELATIVE_PATH _binary_relative_path
          "${_source_dir}" "${_build_dir}")
     IF(NOT _binary_relative_path STREQUAL "." AND
          NOT _binary_relative_path MATCHES "^\\.\\.")
          LIST(APPEND _git_untracked_arguments
               ":(exclude)${_binary_relative_path}"
               ":(exclude)${_binary_relative_path}/**")
     ENDIF()
     EXECUTE_PROCESS(
          COMMAND "${PLUGIN_SDK_GIT_EXECUTABLE}" ${_git_untracked_arguments}
          WORKING_DIRECTORY "${_source_dir}"
          RESULT_VARIABLE _git_untracked_result
          OUTPUT_VARIABLE _git_untracked
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET)
     IF(NOT _git_tracked_result EQUAL 0 OR
          NOT _git_untracked_result EQUAL 0)
          MESSAGE(FATAL_ERROR
               "Unable to recheck the source tree before packaging")
     ENDIF()
     IF(PLUGIN_SDK_SOURCE_TREE_STATE STREQUAL "clean" AND
          (NOT "${_git_tracked}" STREQUAL "" OR
           NOT "${_git_untracked}" STREQUAL ""))
          MESSAGE(FATAL_ERROR
               "The source tree is no longer clean; reconfigure before packaging")
     ENDIF()
ENDIF()

SET(_sdk_root "${_stage_parent}/${PLUGIN_SDK_BASENAME}")
SET(_sdk_archive "${_archive_dir}/${PLUGIN_SDK_BASENAME}.zip")
SET(_sdk_archive_checksum "${_sdk_archive}.sha256")
SET(_payload_files
     "COPYING"
     "README.md"
     "bin/stelMain.dll"
     "include/stellarium/StelMainExport.hpp"
     "include/stellarium/core/StelModule.hpp"
     "include/stellarium/core/StelPluginInterface.hpp"
     "lib/cmake/Stellarium/StellariumConfig.cmake"
     "lib/cmake/Stellarium/StellariumConfigVersion.cmake"
     "lib/stelMain.lib"
     "share/stellarium/plugin-sdk/StellariumPluginBuildInfo.json")

FILE(REMOVE_RECURSE "${_sdk_root}")
FILE(REMOVE "${_sdk_archive}" "${_sdk_archive_checksum}")
FILE(MAKE_DIRECTORY "${_sdk_root}" "${_archive_dir}")

EXECUTE_PROCESS(
     COMMAND "${CMAKE_COMMAND}" --install "${_build_dir}"
          --config "${PLUGIN_SDK_CONFIGURATION}"
          --component "${PLUGIN_SDK_COMPONENT}"
          --prefix "${_sdk_root}"
     RESULT_VARIABLE _install_result)
IF(NOT _install_result EQUAL 0)
     MESSAGE(FATAL_ERROR
          "Plug-in SDK component install failed: ${_install_result}")
ENDIF()

FOREACH(_relative_path IN LISTS _payload_files)
     SET(_payload_path "${_sdk_root}/${_relative_path}")
     IF(NOT EXISTS "${_payload_path}" OR IS_DIRECTORY "${_payload_path}")
          MESSAGE(FATAL_ERROR
               "Plug-in SDK payload file is missing: ${_relative_path}")
     ENDIF()
     FILE(SIZE "${_payload_path}" _payload_size)
     IF(_payload_size EQUAL 0)
          MESSAGE(FATAL_ERROR
               "Plug-in SDK payload file is empty: ${_relative_path}")
     ENDIF()
ENDFOREACH()

EXECUTE_PROCESS(
     COMMAND "${CMAKE_COMMAND}" -E sha256sum ${_payload_files}
     WORKING_DIRECTORY "${_sdk_root}"
     OUTPUT_FILE "${_sdk_root}/MANIFEST.sha256"
     RESULT_VARIABLE _manifest_result)
IF(NOT _manifest_result EQUAL 0)
     MESSAGE(FATAL_ERROR
          "Plug-in SDK checksum manifest failed: ${_manifest_result}")
ENDIF()
FILE(SIZE "${_sdk_root}/MANIFEST.sha256" _manifest_size)
IF(_manifest_size EQUAL 0)
     MESSAGE(FATAL_ERROR "Plug-in SDK checksum manifest is empty")
ENDIF()

SET(_expected_files ${_payload_files} "MANIFEST.sha256")
FILE(GLOB_RECURSE _actual_files
     LIST_DIRECTORIES FALSE
     RELATIVE "${_sdk_root}"
     "${_sdk_root}/*")
LIST(SORT _expected_files)
LIST(SORT _actual_files)
IF(NOT "${_actual_files}" STREQUAL "${_expected_files}")
     MESSAGE(FATAL_ERROR
          "Unexpected plug-in SDK payload. Expected: ${_expected_files}; actual: ${_actual_files}")
ENDIF()

EXECUTE_PROCESS(
     COMMAND "${CMAKE_COMMAND}" -E tar cf "${_sdk_archive}"
          --format=zip -- "${PLUGIN_SDK_BASENAME}"
     WORKING_DIRECTORY "${_stage_parent}"
     RESULT_VARIABLE _archive_result)
IF(NOT _archive_result EQUAL 0)
     MESSAGE(FATAL_ERROR
          "Plug-in SDK archive failed: ${_archive_result}")
ENDIF()
IF(NOT EXISTS "${_sdk_archive}" OR IS_DIRECTORY "${_sdk_archive}")
     MESSAGE(FATAL_ERROR "Plug-in SDK archive was not created")
ENDIF()
FILE(SIZE "${_sdk_archive}" _archive_size)
IF(_archive_size EQUAL 0)
     MESSAGE(FATAL_ERROR "Plug-in SDK archive is empty")
ENDIF()

GET_FILENAME_COMPONENT(_archive_name "${_sdk_archive}" NAME)
EXECUTE_PROCESS(
     COMMAND "${CMAKE_COMMAND}" -E sha256sum "${_archive_name}"
     WORKING_DIRECTORY "${_archive_dir}"
     OUTPUT_FILE "${_sdk_archive_checksum}"
     RESULT_VARIABLE _archive_hash_result)
IF(NOT _archive_hash_result EQUAL 0)
     MESSAGE(FATAL_ERROR
          "Plug-in SDK archive checksum failed: ${_archive_hash_result}")
ENDIF()
FILE(SIZE "${_sdk_archive_checksum}" _archive_hash_size)
IF(_archive_hash_size EQUAL 0)
     MESSAGE(FATAL_ERROR "Plug-in SDK archive checksum is empty")
ENDIF()

MESSAGE(STATUS "Created plug-in SDK archive: ${_sdk_archive}")
