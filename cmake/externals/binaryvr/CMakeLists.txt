include(ExternalProject)

set(EXTERNAL_NAME binaryvr)

string(TOUPPER ${EXTERNAL_NAME} EXTERNAL_NAME_UPPER)

set(BINARYVR_URL "https://s3.ap-northeast-2.amazonaws.com/binaryvrsdk.binaryvr/binaryvr_sdk_hifi.zip")
set(BINARYVR_URL_MD5 "13a871b6e01bf76438933971c378ff56")

ExternalProject_Add(
  ${EXTERNAL_NAME}
  URL ${BINARYVR_URL}
  URL_MD5 ${BINARYVR_URL_MD5}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  LOG_DOWNLOAD 1
)

set_target_properties(${EXTERNAL_NAME} PROPERTIES FOLDER "hidden/externals")

ExternalProject_Get_Property(${EXTERNAL_NAME} SOURCE_DIR)

set(${EXTERNAL_NAME_UPPER}_INCLUDE_DIRS ${SOURCE_DIR}/include CACHE TYPE INTERNAL)
set(${EXTERNAL_NAME_UPPER}_SOURCE_DIR ${SOURCE_DIR} PARENT_SCOPE)

if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8) # support Win64 only

    set(ARCH_DIR ${SOURCE_DIR}/windows/x86_64)
    set(${EXTERNAL_NAME_UPPER}_DLL_PATH ${ARCH_DIR})
    set(${EXTERNAL_NAME_UPPER}_LIB_PATH ${ARCH_DIR})

    set(${EXTERNAL_NAME_UPPER}_LIBRARY_RELEASE "${${EXTERNAL_NAME_UPPER}_LIB_PATH}/binaryfacehmd.lib" CACHE TYPE INTERNAL)
    add_paths_to_fixup_libs("${${EXTERNAL_NAME_UPPER}_DLL_PATH}")
endif()
