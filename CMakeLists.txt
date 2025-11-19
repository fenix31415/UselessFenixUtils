cmake_minimum_required(VERSION 3.20)

option(SKYRIM_SE "The only one true Skyrim SE (1.5) build" ON)
option(WITH_IMGUI "Build with ImGui utilities" OFF)
option(WITH_DRAWING "Build with drawing utilities" OFF)

if(SKYRIM_SE)
  list(APPEND VCPKG_MANIFEST_FEATURES "support-se")
else()
  list(APPEND VCPKG_MANIFEST_FEATURES "support-ae")
endif()

if(WITH_IMGUI)
  list(APPEND VCPKG_MANIFEST_FEATURES "imgui")
endif()

set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")

# ================================================================================

set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_LIST_DIR}/overlay_ports")

if(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})
	if(EXISTS $ENV{VCPKG_ROOT})
		cmake_path(SET CMAKE_TOOLCHAIN_FILE NORMALIZE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
	else()
		message(FATAL_ERROR "invalid VCPKG_ROOT")
	endif()
	
    set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_LIST_DIR}/overlay_ports")


	# install deps from vcpkg.json
	include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
endif()

message("CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")

# ================================================================================
# Project

project(
	UselessFenixUtils
	VERSION 1.0.0
	LANGUAGES CXX
)

include(GNUInstallDirs)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(
		FATAL_ERROR
			"In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
	)
endif()

# ---- Globals ----

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(Boost_USE_STATIC_LIBS ON)

# ---- Add source files ----

set(SOURCES
	src/PCH.h
	include/UselessFenixUtils.h
	src/UselessFenixUtils.cpp
)

if (WITH_IMGUI)
	list(APPEND SOURCES
		include/UselessImguiUtils.h
	)
endif ()

if (WITH_DRAWING)
	list(APPEND SOURCES
		include/UselessDebugRenderUtils.h
		src/DebugRenderUtils.cpp
	)
endif ()

source_group(
	TREE
		${CMAKE_CURRENT_SOURCE_DIR}
	FILES
		${SOURCES}
)

source_group(
	TREE
		${CMAKE_CURRENT_BINARY_DIR}
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
)

# ---- Create DLL ----

add_library(
	${PROJECT_NAME}
	STATIC
	${SOURCES}
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	.clang-format
	.editorconfig
)

#add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

set_target_properties(
	${PROJECT_NAME}
	PROPERTIES
	MSVC_RUNTIME_LIBRARY
		"MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

target_compile_features(
	${PROJECT_NAME}
	PRIVATE
		cxx_std_20
)

target_include_directories(
	${PROJECT_NAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/render>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/utilities>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/Havok>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog REQUIRED CONFIG)
find_package(jsoncpp REQUIRED CONFIG)
find_package(magic_enum CONFIG REQUIRED)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		CommonLibSSE::CommonLibSSE
		spdlog::spdlog
		magic_enum::magic_enum
		JsonCpp::JsonCpp
)

if (WITH_DRAWING)
	target_compile_definitions(${PROJECT_NAME} PUBLIC USELESS_FENIX_UTILS_WITH_DRAWING=1)
endif ()

if (WITH_IMGUI)
	find_package(imgui CONFIG REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
	target_compile_definitions(${PROJECT_NAME} PUBLIC USELESS_FENIX_UTILS_WITH_IMGUI=1)
endif ()

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "-DUNICODE" "-D_UNICODE")

	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/sdl	# Enable Additional Security Checks
			/utf-8	# Set Source and Executable character sets to UTF-8
			/Zi	# Debug Information Format

			/permissive-	# Standards conformance

			/Zc:alignedNew	# C++17 over-aligned allocation
			/Zc:auto	# Deduce Variable Type
			/Zc:char8_t
			/Zc:__cplusplus	# Enable updated __cplusplus macro
			/Zc:externC
			/Zc:externConstexpr	# Enable extern constexpr variables
			/Zc:forScope	# Force Conformance in for Loop Scope
			/Zc:hiddenFriend
			/Zc:implicitNoexcept	# Implicit Exception Specifiers
			/Zc:lambda
			/Zc:noexceptTypes	# C++17 noexcept rules
			/Zc:preprocessor	# Enable preprocessor conformance mode
			/Zc:referenceBinding	# Enforce reference binding rules
			/Zc:rvalueCast	# Enforce type conversion rules
			/Zc:sizedDealloc	# Enable Global Sized Deallocation Functions
			/Zc:strictStrings	# Disable string literal type conversion
			/Zc:ternary	# Enforce conditional operator rules
			/Zc:threadSafeInit	# Thread-safe Local Static Initialization
			/Zc:tlsGuards
			/Zc:trigraphs	# Trigraphs Substitution
			/Zc:wchar_t	# wchar_t Is Native Type

			/external:anglebrackets
			/external:W0

			/W4	# Warning level
			/WX	# Warning level (warnings are errors)

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	if (MSVC)
		if (NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
			target_compile_options(
				${PROJECT_NAME}
				PRIVATE
					/MP	# Build with Multiple Processes
			)
		endif ()
	endif ()

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			/WX	# Treat Linker Warnings as Errors

			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif ()

target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
		src/PCH.h
)

# Установка библиотек и экспорт
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-export
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}
)

# Установка заголовочных файлов
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
    FILES_MATCHING PATTERN "*.h"
)

# Конфигурация пакета для find_package
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/UselessFenixUtils-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/UselessFenixUtils-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Установка файла конфигурации
install(
    FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/UselessFenixUtils-config.cmake
    DESTINATION 
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Установка экспорта целей
install(
    EXPORT ${PROJECT_NAME}-export
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    FILE UselessFenixUtils-targets.cmake
)

