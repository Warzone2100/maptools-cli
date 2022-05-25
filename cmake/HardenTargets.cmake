#
# Copyright © 2018-2022 pastdue ( https://github.com/past-due/ ) and contributors
# License: MIT License ( https://opensource.org/licenses/MIT )
#
# Script Version: 2022-05-24a
#

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (CMAKE_VERSION VERSION_LESS 3.14)
	# CMake < 3.14 doesn't add "-pie" by default for executables (CMake issue #14983)
	if(UNIX AND NOT (APPLE OR ANDROID))
		CHECK_CXX_LINKER_FLAGS("${CMAKE_EXE_LINKER_FLAGS} -pie" LINK_FLAG_PIE_SUPPORTED)
		if(LINK_FLAG_PIE_SUPPORTED)
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
		endif()
	endif()
elseif(POLICY CMP0083)
	cmake_policy(SET CMP0083 NEW) # Apply -pie link flag if supported
endif()

include(AddTargetLinkFlagsIfSupported)

# HARDEN_LINK_FLAGS(TARGET <target>)
#
function(HARDEN_LINK_FLAGS)
	set(_options)
	set(_oneValueArgs TARGET)
	set(_multiValueArgs)

	CMAKE_PARSE_ARGUMENTS(_parsedArguments "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})

	if(NOT DEFINED _parsedArguments_TARGET)
		message( FATAL_ERROR "ADD_TARGET_LINK_FLAGS_IF_SUPPORTED requires TARGET" )
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "Windows")
		# Enable Data Execution Prevention and Address Space Layout Randomization

		if(MSVC)
			ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "/NXCOMPAT" CACHED_RESULT_NAME LINK_FLAG_SLASH_NXCOMPAT_SUPPORTED)
			ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "/DYNAMICBASE" CACHED_RESULT_NAME LINK_FLAG_SLASH_DYNAMICBASE_SUPPORTED)
		endif()

		if(NOT MSVC)
			ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,--nxcompat" CACHED_RESULT_NAME LINK_FLAG_WL_NXCOMPAT_SUPPORTED)
			ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,--dynamicbase" CACHED_RESULT_NAME LINK_FLAG_WL_DYNAMICBASE_SUPPORTED)
			if(CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SIZEOF_VOID_P GREATER 8)
				ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,--high-entropy-va" CACHED_RESULT_NAME LINK_FLAG_WL_HIGHENTROPYVA_SUPPORTED)
			endif()
		endif()

		if(MINGW)
			# Fix: Allow DATA imports from a DLL with a non-zero offset
			ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,--enable-runtime-pseudo-reloc" CACHED_RESULT_NAME LINK_FLAG_WL_RUNTIME_PSEUDO_RELOC_SUPPORTED)
			# Fix: Disable automatic image base calculation (not needed because of ASLR)
			# See: https://lists.ffmpeg.org/pipermail/ffmpeg-cvslog/2015-September/094018.html
			# See: https://sourceware.org/bugzilla/show_bug.cgi?id=19011
			ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,--disable-auto-image-base" CACHED_RESULT_NAME LINK_FLAG_WL_DISABLE_AUTO_IMAGE_BASE_SUPPORTED)
			# Workaround a weird bug with relocation information by enabling --pic-executable
			# See: https://sourceforge.net/p/mingw-w64/mailman/message/31035280/
			ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,--pic-executable" CACHED_RESULT_NAME LINK_FLAG_WL_PICEXECUTABLE_SUPPORTED)
			# Fix: MinGW's LD forgets the entry point when used with pic-executable.
			# See: https://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=91b668a
			if(CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SIZEOF_VOID_P GREATER 8)
				set_property(TARGET ${_parsedArguments_TARGET} APPEND_STRING PROPERTY "LINK_FLAGS" " -Wl,-e,mainCRTStartup")
			else()
				set_property(TARGET ${_parsedArguments_TARGET} APPEND_STRING PROPERTY "LINK_FLAGS" " -Wl,-e,_mainCRTStartup")
			endif()
			# Fix: Opt-in to extra entropy when using High-Entropy ASLR by setting the image base > 4GB
			# See: https://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=a58c22d
			if(CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SIZEOF_VOID_P GREATER 8)
				# Note: The image base should be:
				#	0x140000000 - for exes
				#	0x180000000 - for DLLs
				set_property(TARGET ${_parsedArguments_TARGET} APPEND_STRING PROPERTY "LINK_FLAGS" " -Wl,--image-base,0x140000000")
			endif()
		endif()
	endif()

	if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin" AND NOT MSVC)
		# Ensure noexecstack
		ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,-z,noexecstack" CACHED_RESULT_NAME LINK_FLAG_WL_Z_NOEXECSTACK_SUPPORTED)
		# Enable RELRO (if supported)
		ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,-z,relro" CACHED_RESULT_NAME LINK_FLAG_WL_Z_RELRO_SUPPORTED)
		ADD_TARGET_LINK_FLAGS_IF_SUPPORTED(TARGET ${_parsedArguments_TARGET} LINK_FLAGS "-Wl,-z,now" CACHED_RESULT_NAME LINK_FLAG_WL_Z_NOW_SUPPORTED)
	endif()

endfunction()

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

macro(HARDEN_C_FLAGS)

	# Enable stack protection, if supported by the compiler
	# Prefer -fstack-protector-strong if supported, fall-back to -fstack-protector
	check_c_compiler_flag(-fstack-protector-strong HAS_CFLAG_FSTACK_PROTECTOR_STRONG)
	if (HAS_CFLAG_FSTACK_PROTECTOR_STRONG)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
	else()
		check_c_compiler_flag(-fstack-protector HAS_CFLAG_FSTACK_PROTECTOR)
		if (HAS_CFLAG_FSTACK_PROTECTOR)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
		endif()
	endif()

	# Enable -fstack-clash-protection if available
	# Note: https://gitlab.kitware.com/cmake/cmake/-/issues/21998
	set(_old_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
	check_c_compiler_flag(-fstack-clash-protection HAS_CFLAG_FSTACK_CLASH_PROTECTION)
	set(CMAKE_C_FLAGS "${_old_CMAKE_C_FLAGS}")
	if (HAS_CFLAG_FSTACK_CLASH_PROTECTION AND NOT MINGW)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-clash-protection")
	endif()

endmacro()

macro(HARDEN_CXX_FLAGS)

	# Enable stack protection, if supported by the compiler
	# Prefer -fstack-protector-strong if supported, fall-back to -fstack-protector
	check_cxx_compiler_flag(-fstack-protector-strong HAS_CXXFLAG_FSTACK_PROTECTOR_STRONG)
	if (HAS_CXXFLAG_FSTACK_PROTECTOR_STRONG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
	else()
		check_cxx_compiler_flag(-fstack-protector HAS_CXXFLAG_FSTACK_PROTECTOR)
		if (HAS_CXXFLAG_FSTACK_PROTECTOR)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
		endif()
	endif()

	# Enable -fstack-clash-protection if available
	# Note: https://gitlab.kitware.com/cmake/cmake/-/issues/21998
	set(_old_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	check_cxx_compiler_flag(-fstack-clash-protection HAS_CXXFLAG_FSTACK_CLASH_PROTECTION)
	set(CMAKE_CXX_FLAGS "${_old_CMAKE_CXX_FLAGS}")
	if (HAS_CXXFLAG_FSTACK_CLASH_PROTECTION AND NOT MINGW)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-clash-protection")
	endif()

endmacro()

if (CMAKE_C_COMPILER_LOADED)
	HARDEN_C_FLAGS()
endif()

if (CMAKE_CXX_COMPILER_LOADED)
	HARDEN_CXX_FLAGS()
endif()
