if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET AND NOT "$ENV{VCPKG_DEFAULT_TRIPLET}" STREQUAL "")
	set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)

	set(VCPKG_ROOT "${CMAKE_CURRENT_BINARY_DIR}/vcpkg")
	find_package(Git REQUIRED)

	if(NOT EXISTS "${VCPKG_ROOT}")
		message(STATUS "Cloning vcpkg in: ${VCPKG_ROOT}")
		execute_process(
			COMMAND ${GIT_EXECUTABLE} clone https://github.com/Microsoft/vcpkg.git "${VCPKG_ROOT}"
			RESULT_VARIABLE _clone_result
		)
		if(NOT _clone_result EQUAL 0)
			message(FATAL_ERROR "Failed to clone vcpkg in: ${VCPKG_ROOT}")
		endif()
	else()
		message(STATUS "Updating vcpkg in: ${VCPKG_ROOT}")
		execute_process(
			COMMAND ${GIT_EXECUTABLE} pull
			WORKING_DIRECTORY "${VCPKG_ROOT}"
		)
	endif()

	if(NOT EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
		message(FATAL_ERROR "vcpkg bootstrap failure; path not found: \"${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake\"")
	endif()

	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

endif()
