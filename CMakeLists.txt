set(_cmake_min_ver_supported 3.5)
if (TARGET wzmaplib)
	# This should happen if being included in the larger WZ build scripts, thus we don't need FetchContent and can support earlier CMake
	message(DEBUG "wzmaplib is already available - no need to use FetchContent")
else()
	message(STATUS "maptools: wzmaplib must be fetched")
	set(_cmake_min_ver_supported 3.11) # CMake 3.11 is the minimum version for FetchContent
endif()

cmake_minimum_required(VERSION ${_cmake_min_ver_supported}...3.14)

set(_OLD_CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(VcpkgInit) # Must come before project() command
set(CMAKE_MODULE_PATH "${_OLD_CMAKE_MODULE_PATH}")
unset(_OLD_CMAKE_MODULE_PATH)

project (maptools CXX)
enable_language(C)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(HardenTargets)

set(BUILD_SHARED_LIBS OFF)

# Support two methods of maptools compilation:
# 1.) Embedded as a submodule in the WZ main project + buildsystem
# 2.) Compiled stand-alone
if(NOT TARGET wzmaplib)
	# Not embedded in parent main WZ build system - must manually obtain wzmaplib and include it
	# Get a specific version of wzmaplib (from the warzone2100 repo, currently)
	include(FetchContent)
	message(STATUS "maptools: Fetching wzmaplib (from warzone2100 repo - this may take a while)")
	FetchContent_Declare(
		warzone2100_repo
		GIT_REPOSITORY https://github.com/Warzone2100/warzone2100.git
		GIT_TAG        0c0e6c466861fb22eaf43bf92a8914d6ea146ced
		GIT_PROGRESS   TRUE
		USES_TERMINAL_DOWNLOAD	TRUE
	)
	FetchContent_GetProperties(warzone2100_repo)
	if(NOT warzone2100_repo_POPULATED)
		FetchContent_Populate(warzone2100_repo)
	endif()
	message(STATUS "maptools: Adding wzmaplib")
	add_subdirectory("${warzone2100_repo_SOURCE_DIR}/lib/wzmaplib" "lib/wzmaplib" EXCLUDE_FROM_ALL)
endif()

find_package(PNG 1.2 REQUIRED)

set (CMAKE_USE_PTHREADS_INIT TRUE)
find_package(Threads)

# set CMAKE_BUILD_TYPE to default
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif ()

#####################
# maptools

add_executable(maptools
				src/maptools.cpp src/pngsave.cpp src/pngsave.h src/maptools_version.cpp src/maptools_version.h)
set_target_properties(maptools
	PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
)
target_include_directories(maptools PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty")
target_link_libraries(maptools PRIVATE wzmaplib PNG::PNG)
target_link_libraries(maptools PRIVATE nlohmann_json)
if (TARGET ZipIOProvider)
	target_link_libraries(maptools PRIVATE ZipIOProvider)
else()
	message(WARNING "ZipIOProvider is not available - please ensure libzip is installed. maptools will be compiled without direct support for .wz archives")
	target_compile_definitions(maptools PRIVATE "WZ_MAPTOOLS_DISABLE_ARCHIVE_SUPPORT")
endif()

if(MSVC)
	target_compile_definitions(maptools PRIVATE "_CRT_SECURE_NO_WARNINGS")
endif()

file(READ "VERSION.in" ver)
STRING(REGEX MATCH "^([0-9]+).([0-9]+).([0-9]+)" MAPTOOLS_CLI_VERSION_STRING "${ver}")
if(NOT MAPTOOLS_CLI_VERSION_STRING)
	message(FATAL_ERROR "VERSION.in missing three-component version?")
endif()
set(MAPTOOLS_CLI_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(MAPTOOLS_CLI_VERSION_MINOR "${CMAKE_MATCH_2}")
set(MAPTOOLS_CLI_VERSION_REV "${CMAKE_MATCH_3}")
target_compile_definitions(maptools PRIVATE "MAPTOOLS_CLI_VERSION_MAJOR=${MAPTOOLS_CLI_VERSION_MAJOR}")
target_compile_definitions(maptools PRIVATE "MAPTOOLS_CLI_VERSION_MINOR=${MAPTOOLS_CLI_VERSION_MINOR}")
target_compile_definitions(maptools PRIVATE "MAPTOOLS_CLI_VERSION_REV=${MAPTOOLS_CLI_VERSION_REV}")

HARDEN_LINK_FLAGS(TARGET maptools)

############################
# App install location

# To change the install destination at configure-time, please change the value of CMAKE_INSTALL_BINDIR
set(WZ_APP_INSTALL_DEST "${CMAKE_INSTALL_BINDIR}")

############################
# Install
install(TARGETS maptools DESTINATION "${WZ_APP_INSTALL_DEST}")

############################
# Installing Required Runtime Dependencies

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	set(_mainexename "maptools")
	if(_mainexename)
		if(NOT CMAKE_CROSSCOMPILING)
			# Install any required runtime dependencies / DLLs (ex. from vcpkg when dynamically linking)
			set(_wz_fixup_bundle_ignored_filenames)
			set(_wz_fixup_bundle_nocopy_libraries)
			if(MSVC)
				# Ignore system (CRT) runtimes in fixup_bundle
				# - Get a list of all of the required system libraries
				set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
				set(CMAKE_INSTALL_DEBUG_LIBRARIES FALSE)
				set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
				set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS TRUE)
				include(InstallRequiredSystemLibraries)
				# - CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS should now contain the runtime files (full paths)
				# - Extract just the filenames
				foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
					get_filename_component(lib_name "${lib}" NAME)
					list(APPEND _wz_fixup_bundle_ignored_filenames "${lib_name}")
					list(APPEND _wz_fixup_bundle_nocopy_libraries "${lib_name}")
				endforeach()
				if(NOT _wz_fixup_bundle_ignored_filenames)
					message( WARNING "InstallRequiredSystemLibraries returned no libraries? (CMake: ${CMAKE_VERSION}; MSVC: ${MSVC_VERSION})" )
				endif()
			endif()
			# - Always ignore dbghelp.dll
			list(APPEND _wz_fixup_bundle_ignored_filenames "dbghelp.dll")
			list(APPEND _wz_fixup_bundle_nocopy_libraries "dbghelp.dll")
			if(MINGW)
				# Find path to C++ stdlib, and preserve for later fixup_bundle use
				find_file(WZ_STDCXXDLL NAMES "libc++.dll" "libstdc++-6.dll")
				if(WZ_STDCXXDLL)
					message(STATUS "Detected C++ stdlib DLL: ${WZ_STDCXXDLL}")
					get_filename_component(WZ_STDCXXDLL_PATH "${WZ_STDCXXDLL}" DIRECTORY)
				else()
					message(WARNING "Did not find a path to the C++ stdlib DLL. Packaged binary may not run on other systems.")
				endif()
			endif()
			if(_wz_fixup_bundle_ignored_filenames)
				message( STATUS "fixup_bundle: IGNORE_ITEM ${_wz_fixup_bundle_ignored_filenames}" )
			endif()
			install(CODE "
				execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++install CODE: CMAKE_INSTALL_CONFIG_NAME: \${CMAKE_INSTALL_CONFIG_NAME}\")
				execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++install CODE: CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}\")
				# vcpkg's vcpkg.cmake adds both the release and debug prefix paths to the CMAKE_PREFIX_PATH:
				# - ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug # (debug prefix)
				# - ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET} # (release prefix)
				# the debug path is a subdirectory of the main / release path
				# remove the debug path, leaving just the base prefix
				set(wz_vcpkg_installed_prefix_path \"${CMAKE_PREFIX_PATH}\")
				list(FILTER wz_vcpkg_installed_prefix_path EXCLUDE REGEX \"/debug$\")
				execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++install CODE: vcpkg_installed_prefix_path: \${wz_vcpkg_installed_prefix_path}\")
				if(\"\${CMAKE_INSTALL_CONFIG_NAME}\" MATCHES \"^([Dd][Ee][Bb][Uu][Gg])$\")
					set(dll_source_dirs \"\${wz_vcpkg_installed_prefix_path}/debug/bin/\")
				else()
					set(dll_source_dirs \"\${wz_vcpkg_installed_prefix_path}/bin/\")
				endif()
				# MINGW may require distributing the C++ stdlib DLLs
				set(_WZ_STDCXXDLL_PATH \"${WZ_STDCXXDLL_PATH}\")
				if(_WZ_STDCXXDLL_PATH AND EXISTS \"\${_WZ_STDCXXDLL_PATH}\")
					execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++install CODE: C++ stdlib path: \${_WZ_STDCXXDLL_PATH}\")
					# Add second, to ensure that vcpkg-built dependencies are always selected first
					list(APPEND dll_source_dirs \"\${_WZ_STDCXXDLL_PATH}\")
				endif()
				execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++install CODE: dll_source_dirs: \${dll_source_dirs}\")
				set(_ignored_filenames \"${_wz_fixup_bundle_ignored_filenames}\")
				if(_ignored_filenames)
					set(_wz_fixup_bundle_ignore_item \"IGNORE_ITEM \\\"\${_ignored_filenames}\\\"\")
				else()
					set(_wz_fixup_bundle_ignore_item)
				endif()
				set(BU_CHMOD_BUNDLE_ITEMS ON)
				include(BundleUtilities)
				fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${WZ_APP_INSTALL_DEST}/${_mainexename}.exe\" \"\" \"\${dll_source_dirs}\" ${_wz_fixup_bundle_ignore_item})
				# Passing IGNORE_ITEM to fixup_bundle does not prevent fixup_bundle from copying the ignored items themselves to the BINDIR
				# Iterate over _wz_fixup_bundle_nocopy_libraries and remove them if they've been copied
				set(_nocopy_libs \"${_wz_fixup_bundle_nocopy_libraries}\")
				foreach(lib \${_nocopy_libs})
					set(_lib_fullpath \"\${CMAKE_INSTALL_PREFIX}/${WZ_APP_INSTALL_DEST}/\${lib}\")
					if(EXISTS \"\${_lib_fullpath}\")
						execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++Removing lib: \${lib}\")
						file(REMOVE \"\${_lib_fullpath}\")
					endif()
				endforeach()
				# Sanity-check to ensure that the C++ stdlib, for mingw, actually came from mingw (i.e. is the expected version)
				set(_WZ_STDCXXDLL \"${WZ_STDCXXDLL}\")
				if(_WZ_STDCXXDLL AND EXISTS \"\${_WZ_STDCXXDLL}\")
					file(SHA512 \"\${_WZ_STDCXXDLL}\" _std_cxx_expected_hash)
					get_filename_component(_WZ_STDCXXDLL_FILENAME \"\${_WZ_STDCXXDLL}\" NAME)
					set(_copied_cxx_stdlib \"\${CMAKE_INSTALL_PREFIX}/${WZ_APP_INSTALL_DEST}/\${_WZ_STDCXXDLL_FILENAME}\")
					file(SHA512 \"\${_copied_cxx_stdlib}\" _std_cxx_actual_hash)
					if(\"\${_std_cxx_expected_hash}\" STREQUAL \"\${_std_cxx_actual_hash}\")
						execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++Verified expected C++ stdlib: \${_WZ_STDCXXDLL_FILENAME}\")
					else()
						message(FATAL_ERROR \"Copied C++ stdlib does not match expected C++ stdlib\")
					endif()
					if(NOT CMAKE_VERSION VERSION_LESS 3.16) # file(GET_RUNTIME_DEPENDENCIES ...) requires CMake 3.16+
						file(GET_RUNTIME_DEPENDENCIES
							RESOLVED_DEPENDENCIES_VAR _WZ_STDCXXDLL_DEPENDENCIES
							UNRESOLVED_DEPENDENCIES_VAR _WZ_STDCXXDLL_UNRESOLVED_DEPENDENCIES
							LIBRARIES \"\${_WZ_STDCXXDLL}\"
							DIRECTORIES \"\${_WZ_STDCXXDLL_PATH}\"
							PRE_EXCLUDE_REGEXES \"^api-ms-win-*.dll$\" \"^ucrtbase.dll$\" \"^kernelbase.dll$\" \"^kernel32.dll$\"
							POST_EXCLUDE_REGEXES \"[\\\\/][Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\\\/].*\.dll$\"
						)
						foreach(_stdcxx_dependency IN LISTS _WZ_STDCXXDLL_DEPENDENCIES)
							get_filename_component(_stdcxx_dependency_filename \"\${_stdcxx_dependency}\" NAME)
							if(EXISTS \"\${_WZ_STDCXXDLL_PATH}/\${_stdcxx_dependency_filename}\")
								file(SHA512 \"\${_WZ_STDCXXDLL_PATH}/\${_stdcxx_dependency_filename}\" _stdcxx_dependency_expected_hash)
								if(EXISTS \"\${_WZ_STDCXXDLL_PATH}/\${_stdcxx_dependency_filename}\")
									file(SHA512 \"\${CMAKE_INSTALL_PREFIX}/${WZ_APP_INSTALL_DEST}/\${_stdcxx_dependency_filename}\" _stdcxx_dependency_actual_hash)
									if(\"\${_stdcxx_dependency_expected_hash}\" STREQUAL \"\${_stdcxx_dependency_actual_hash}\")
										execute_process(COMMAND \${CMAKE_COMMAND} -E echo \"++Verified expected C++ stdlib dependency: \${_stdcxx_dependency_filename}\")
									else()
										message(FATAL_ERROR \"Copied C++ stdlib dependency does not match expected: \${_stdcxx_dependency_filename}\")
									endif()
								endif()
							endif()
						endforeach()
					endif()
				endif()
			")

		else()
			message( STATUS "CMAKE_CROSSCOMPILING is defined - skipping BundleUtilities" )
		endif()

	else()
		message( WARNING "Unable to get OUTPUT_NAME from maptools target" )
	endif()
endif()

############################
# Packaging

set(CPACK_PACKAGE_NAME			"maptools CLI")
set(CPACK_PACKAGE_VENDOR		"Warzone 2100 Project")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CHECKSUM		SHA256)
set(CPACK_PACKAGE_HOMEPAGE_URL  "https://wz2100.net")

if(NOT DEFINED CPACK_PACKAGE_FILE_NAME)
	# Default of "maptools"
	set(CPACK_PACKAGE_FILE_NAME "maptools")
endif()

if(NOT DEFINED CPACK_SOURCE_PACKAGE_FILE_NAME)
	# Default of "maptools"
	set(CPACK_SOURCE_PACKAGE_FILE_NAME "maptools")
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE)

# Ignore version control
list (APPEND CPACK_SOURCE_IGNORE_FILES "/\\\\.git")
# Ignore various CI / Github-config related files
list (APPEND CPACK_SOURCE_IGNORE_FILES "/\\\\.github/")
# Ignore development / runtime created files
list (APPEND CPACK_SOURCE_IGNORE_FILES "~$")
list (APPEND CPACK_SOURCE_IGNORE_FILES "\\\\.DS_Store$")

include(CPack)
