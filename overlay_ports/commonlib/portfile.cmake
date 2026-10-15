vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL git@gitlab.com:skyrp-group/skyrp-commonlib.git
    REF 31d9068c5a6b6305f81b31d9a643cf26d9d38191
    HEAD_REF main
)

set(OPTIONS "")

if("support-ae" IN_LIST FEATURES)
    list(APPEND OPTIONS -DSKYRIM_SUPPORT_AE)
endif()

message(STATUS "Building commonlib with OPTIONS: `${FEATURE_OPTIONS}`")

vcpkg_configure_cmake(
  SOURCE_PATH ${SOURCE_PATH}
  PREFER_NINJA
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
