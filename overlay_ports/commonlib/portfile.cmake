vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO fenix31415/CommonLibSSE
    REF 74694d16f550cdf835f364e37e94fc7dd7372cfc
    SHA512 c13092aa91a86d6282ef56375b7ed1375cd7b379a50a1ecaad864dc71bbe434d765fdc4178e0d542a8fda46a3869fe5b760ca96f538c4e406fa0814c04e5193f
    HEAD_REF dev
)

set(OPTIONS "")

if("support-ae" IN_LIST FEATURES)
    list(APPEND OPTIONS -DSKYRIM_SUPPORT_AE)
endif()

message(STATUS "Building commonlib with OPTIONS: `${FEATURE_OPTIONS}`")

vcpkg_configure_cmake(
  SOURCE_PATH ${SOURCE_PATH}
  OPTIONS
      ${OPTIONS}
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME CommonLibSSE CONFIG_PATH lib/cmake)
vcpkg_copy_pdbs()

file(GLOB CMAKE_CONFIGS "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE/*.cmake")
file(INSTALL ${CMAKE_CONFIGS} DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(
	INSTALL "${SOURCE_PATH}/LICENSE"
	DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
	RENAME copyright)
