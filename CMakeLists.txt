set(_cmake_min_ver_supported 3.5)
if (TARGET wzmaplib)
	# This should happen if being included in the larger WZ build scripts, thus we don't need FetchContent and can support earlier CMake
	message(DEBUG "wzmaplib is already available - no need to use FetchContent")
else()
	message(STATUS "maptools: wzmaplib must be fetched")
	set(_cmake_min_ver_supported 3.11) # CMake 3.11 is the minimum version for FetchContent
endif()

cmake_minimum_required(VERSION ${_cmake_min_ver_supported}...3.14)
project (maptools CXX)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

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
		GIT_TAG        07f03ebe542738a831e328ab282dddb188827e18
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

# maptools
add_executable(maptools
				maptools.cpp pngsave.cpp pngsave.h)
set_target_properties(maptools
	PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
)
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
