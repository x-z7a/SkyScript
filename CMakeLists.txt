CMAKE_MINIMUM_REQUIRED(VERSION 3.25.1)
SET(CMAKE_CXX_STANDARD 23)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
PROJECT(SkyScript C CXX)

IF(MSVC)
    SET(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    STRING(APPEND CMAKE_CXX_FLAGS " /DNDEBUG")
    STRING(APPEND CMAKE_C_FLAGS " /DNDEBUG")
ELSE()
    STRING(APPEND CMAKE_CXX_FLAGS " -O0 -DNDEBUG")
    STRING(APPEND CMAKE_C_FLAGS " -O0 -DNDEBUG")
ENDIF()

IF(NOT DEFINED XPLANE_VERSION)
    MESSAGE(FATAL_ERROR "XPLANE_VERSION not defined")
ENDIF()

IF(NOT XPLANE_VERSION EQUAL 12)
    MESSAGE(FATAL_ERROR "SkyScript only supports X-Plane 12 builds")
ENDIF()

SET(DEFAULT_XPLANE_SDK_ROOT "/Volumes/storage/git/SDK")
SET(XPLANE_SDK_ROOT "${DEFAULT_XPLANE_SDK_ROOT}" CACHE PATH "Path to the X-Plane SDK root")
SET(XPLANE_INCLUDES_PATH "${XPLANE_SDK_ROOT}/CHeaders")

IF(NOT EXISTS "${XPLANE_INCLUDES_PATH}/XPLM")
    MESSAGE(FATAL_ERROR "XPLANE_SDK_ROOT is invalid: ${XPLANE_SDK_ROOT}")
ENDIF()

IF(APPLE)
    SET(DIRECTORY_PREFIX "mac_x64")
ELSEIF(UNIX)
    SET(DIRECTORY_PREFIX "lin_x64")
ELSEIF(WIN32)
    SET(DIRECTORY_PREFIX "win_x64")
ENDIF()

SET(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/${DIRECTORY_PREFIX}/cef")

IF(APPLE)
    SET(XPLM_LIBRARY "${XPLANE_SDK_ROOT}/Libraries/Mac/XPLM.framework/XPLM")
    SET(XPWIDGETS_LIBRARY "${XPLANE_SDK_ROOT}/Libraries/Mac/XPWidgets.framework/XPWidgets")
    SET(CEF_LIBRARY "${CEF_ROOT}/Release/Chromium Embedded Framework.framework/Chromium Embedded Framework")
    SET(CEF_WRAPPER "${CEF_ROOT}/libcef_dll_wrapper.a")

    FOREACH(required_file ${XPLM_LIBRARY} ${XPWIDGETS_LIBRARY} ${CEF_LIBRARY} ${CEF_WRAPPER})
        IF(NOT EXISTS "${required_file}")
            MESSAGE(FATAL_ERROR "Missing required dependency: ${required_file}")
        ENDIF()
    ENDFOREACH()

    FIND_PACKAGE(CURL REQUIRED)
    SET(CURL_LIBRARY CURL::libcurl)
    FIND_LIBRARY(COCOA_LIBRARY Cocoa REQUIRED)
    FIND_LIBRARY(OPENGL_LIBRARY OpenGL REQUIRED)
ELSEIF(WIN32)
    SET(XPLANE_LIBRARY_PATH "${XPLANE_SDK_ROOT}/Libraries/Win")
    FIND_LIBRARY(CEF_LIBRARY NAMES libcef PATHS "${CEF_ROOT}/Release" NO_DEFAULT_PATH REQUIRED)
    FIND_LIBRARY(CEF_WRAPPER NAMES libcef_dll_wrapper PATHS "${CEF_ROOT}/libcef_dll_wrapper" NO_DEFAULT_PATH REQUIRED)
    FIND_LIBRARY(OPENGL_LIBRARY NAMES opengl32 REQUIRED)
    IF(MSVC)
        FIND_PACKAGE(CURL CONFIG REQUIRED)
        SET(CURL_LIBRARY CURL::libcurl)
    ELSE()
        FIND_LIBRARY(CURL_LIBRARY NAMES libcurl PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib/${DIRECTORY_PREFIX}/curl/lib" REQUIRED)
    ENDIF()
    FIND_LIBRARY(XPLM_LIBRARY NAMES XPLM_64 PATHS "${XPLANE_LIBRARY_PATH}" REQUIRED)
    FIND_LIBRARY(XPWIDGETS_LIBRARY NAMES XPWidgets_64 PATHS "${XPLANE_LIBRARY_PATH}" REQUIRED)
ELSEIF(UNIX)
    SET(XPLANE_LIBRARY_PATH "${XPLANE_SDK_ROOT}/Libraries/Lin")
    FIND_PACKAGE(CURL REQUIRED)
    FIND_LIBRARY(OPENSSL_LIBRARY NAMES libssl.so PATHS "/usr/lib/x86_64-linux-gnu")
    FIND_LIBRARY(OPENSSL_CRYPTO_LIBRARY NAMES libcrypto.so PATHS "/usr/lib/x86_64-linux-gnu")
    FIND_LIBRARY(CURL_LIBRARY NAMES libcurl.so PATHS "/usr/lib/x86_64-linux-gnu")
    FIND_LIBRARY(CEF_LIBRARY libcef.so PATHS "${CEF_ROOT}/Release" NO_DEFAULT_PATH)
    SET(CEF_WRAPPER "${CEF_ROOT}/libcef_dll_wrapper/libcef_dll_wrapper.a")
    FIND_PACKAGE(OpenGL REQUIRED)
    FIND_LIBRARY(XPLM_LIBRARY XPLM_64.so PATHS "${XPLANE_LIBRARY_PATH}" REQUIRED)
    FIND_LIBRARY(XPWIDGETS_LIBRARY XPWidgets_64.so PATHS "${XPLANE_LIBRARY_PATH}" REQUIRED)
ENDIF()

FUNCTION(HEADER_DIRECTORIES return_list base_dir)
    FILE(GLOB_RECURSE new_list "${base_dir}/*.h" "${base_dir}/*.hpp")
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        LIST(APPEND dir_list ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list} PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(FIND_SOURCE_FILES return_list base_dir)
    SET(file_patterns "${base_dir}/*.c" "${base_dir}/*.cpp" "${base_dir}/*.hpp")

    IF(APPLE)
        LIST(APPEND file_patterns "${base_dir}/*.mm")
    ENDIF()
    FILE(GLOB_RECURSE new_list ${file_patterns})

    LIST(FIND new_list "${base_dir}/main.cpp" main_index)
    IF(main_index GREATER_EQUAL 0)
        LIST(REMOVE_AT new_list ${main_index})
        LIST(PREPEND new_list "${base_dir}/main.cpp")
    ENDIF()

    SET(${return_list} ${new_list} PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(use_plugin_local_runtime_paths target_name)
    IF(APPLE)
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES
            BUILD_RPATH "@loader_path"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@loader_path"
        )
        ADD_CUSTOM_COMMAND(TARGET ${target_name} POST_BUILD
            COMMAND install_name_tool
                -change
                "@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
                "@loader_path/Chromium Embedded Framework.framework/Chromium Embedded Framework"
                "$<TARGET_FILE:${target_name}>"
            VERBATIM
        )
    ELSEIF(UNIX)
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES
            BUILD_RPATH "\$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "\$ORIGIN"
        )
    ENDIF()
ENDFUNCTION()

FUNCTION(add_xplane_sdk_definitions library_name library_version)
    IF(APPLE)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DAPL=1 -DIBM=0 -DLIN=0 -DXPLANE_VERSION=${XPLANE_VERSION})
    ELSEIF(UNIX)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DAPL=0 -DIBM=0 -DLIN=1 -DXPLANE_VERSION=${XPLANE_VERSION})
        TARGET_COMPILE_OPTIONS(${library_name} PRIVATE "-static")
        TARGET_LINK_OPTIONS(${library_name} PRIVATE "-static-libstdc++" "-static-libgcc")
    ELSEIF(WIN32)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DAPL=0 -DIBM=1 -DLIN=0 -DUSING_CEF_SHARED=1 -DNOMINMAX -DWIN32_LEAN_AND_MEAN -DXPLANE_VERSION=${XPLANE_VERSION})
        IF(MSVC)
            TARGET_COMPILE_OPTIONS(${library_name} PRIVATE "/bigobj")
            TARGET_LINK_OPTIONS(${library_name} PRIVATE "/FORCE:MULTIPLE")
        ELSE()
            TARGET_COMPILE_OPTIONS(${library_name} PRIVATE "-static")
            TARGET_LINK_OPTIONS(${library_name} PRIVATE "-static-libstdc++" "-static-libgcc" "-static" "-Wl,--allow-multiple-definition")
        ENDIF()
    ENDIF()

    IF(library_version EQUAL 200)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1)
    ELSEIF(library_version EQUAL 210)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1 -DXPLM210=1)
    ELSEIF(library_version EQUAL 300)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1 -DXPLM210=1 -DXPLM300=1)
    ELSEIF(library_version EQUAL 301)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1)
    ELSEIF(library_version EQUAL 400)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1 -DXPLM400=1)
    ELSEIF(library_version EQUAL 410)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1 -DXPLM400=1 -DXPLM410=1)
    ELSEIF(library_version EQUAL 420)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1 -DXPLM400=1 -DXPLM410=1 -DXPLM411=1)
    ELSE()
        MESSAGE(FATAL_ERROR "Library version one of: 200, 210, 300, 301, 400, 410, 420")
    ENDIF()

    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC "${XPLANE_INCLUDES_PATH}/XPLM" "${XPLANE_INCLUDES_PATH}/Widgets" "${XPLANE_INCLUDES_PATH}/Wrappers")
ENDFUNCTION()

FUNCTION(add_xplane_plugin library_name library_version src_dir)
    MESSAGE(STATUS "Preparing X-Plane plugin '${library_name}' at ${src_dir}")
    FIND_SOURCE_FILES(FILES ${src_dir})

    IF(APPLE)
        ADD_EXECUTABLE(${library_name} ${FILES})
    ELSEIF(UNIX)
        ADD_LIBRARY(${library_name} SHARED ${FILES})
    ELSEIF(WIN32)
        ADD_LIBRARY(${library_name} MODULE ${FILES})
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC _CRT_SECURE_NO_WARNINGS)
    ENDIF()

    IF(${library_version} LESS 300)
        IF(APPLE)
            SET_TARGET_PROPERTIES(${library_name} PROPERTIES OUTPUT_NAME "mac.xpl")
        ELSEIF(UNIX)
            SET_TARGET_PROPERTIES(${library_name} PROPERTIES OUTPUT_NAME "lin.xpl")
        ELSEIF(WIN32)
            SET_TARGET_PROPERTIES(${library_name} PROPERTIES OUTPUT_NAME "win.xpl")
        ENDIF()
    ELSE()
        IF(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}")
            FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}")
        ENDIF()
        SET_TARGET_PROPERTIES(${library_name} PROPERTIES
            OUTPUT_NAME "${library_name}.xpl"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}"
        )
        FOREACH(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            SET_TARGET_PROPERTIES(${library_name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}"
                LIBRARY_OUTPUT_DIRECTORY_${config} "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}"
                ARCHIVE_OUTPUT_DIRECTORY_${config} "${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY_PREFIX}"
            )
        ENDFOREACH()
    ENDIF()

    add_xplane_sdk_definitions(${library_name} ${library_version})

    SET_TARGET_PROPERTIES(${library_name} PROPERTIES PREFIX "")
    SET_TARGET_PROPERTIES(${library_name} PROPERTIES SUFFIX "")
    SET_TARGET_PROPERTIES(${library_name} PROPERTIES LINKER_LANGUAGE CXX)

    IF(APPLE)
        SET_PROPERTY(TARGET ${library_name} APPEND_STRING PROPERTY LINK_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fPIC -fvisibility=hidden -bundle")
    ELSEIF(WIN32)
        IF(NOT MSVC)
            TARGET_INCLUDE_DIRECTORIES(${library_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/${DIRECTORY_PREFIX}/curl/include")
        ENDIF()
    ELSEIF(UNIX)
        SET_PROPERTY(TARGET ${library_name} APPEND_STRING PROPERTY LINK_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic -nodefaultlibs -undefined_warning -fPIC -fvisibility=hidden")
    ENDIF()

    TARGET_LINK_LIBRARIES(${library_name} PUBLIC ${XPLM_LIBRARY} ${XPWIDGETS_LIBRARY} ${CEF_WRAPPER} ${CEF_LIBRARY} ${CURL_LIBRARY} ${OPENGL_LIBRARY} ${COCOA_LIBRARY} ${OPENSSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC "${XPLANE_INCLUDES_PATH}/XPLM" "${XPLANE_INCLUDES_PATH}/Widgets" "${XPLANE_INCLUDES_PATH}/Wrappers")
    TARGET_INCLUDE_DIRECTORIES(${library_name} PRIVATE "${CEF_ROOT}")
    HEADER_DIRECTORIES(header_dir_list ${src_dir})
    TARGET_INCLUDE_DIRECTORIES(${library_name} PRIVATE ${header_dir_list})
    use_plugin_local_runtime_paths(${library_name})
ENDFUNCTION()

# Build SkyScript as a shared library that other plugins can link against
FUNCTION(add_skyscript_library library_name library_version src_dir)
    MESSAGE(STATUS "Preparing SkyScript library '${library_name}' at ${src_dir}")
    FIND_SOURCE_FILES(FILES ${src_dir})

    ADD_LIBRARY(${library_name} SHARED ${FILES})
    SET_TARGET_PROPERTIES(${library_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    TARGET_COMPILE_DEFINITIONS(${library_name} PRIVATE SKYSCRIPT_BUILDING_DLL)

    add_xplane_sdk_definitions(${library_name} ${library_version})

    SET_TARGET_PROPERTIES(${library_name} PROPERTIES LINKER_LANGUAGE CXX)

    IF(APPLE)
        SET_TARGET_PROPERTIES(${library_name} PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            INSTALL_NAME_DIR "@rpath"
            MACOSX_RPATH ON
            VISIBILITY_INLINES_HIDDEN ON
        )
    ELSEIF(UNIX)
        SET_TARGET_PROPERTIES(${library_name} PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )
    ENDIF()

    IF(WIN32)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC _CRT_SECURE_NO_WARNINGS)
        IF(NOT MSVC)
            TARGET_INCLUDE_DIRECTORIES(${library_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/${DIRECTORY_PREFIX}/curl/include")
        ENDIF()
    ENDIF()

    TARGET_LINK_LIBRARIES(${library_name} PUBLIC ${XPLM_LIBRARY} ${XPWIDGETS_LIBRARY} ${CEF_WRAPPER} ${CEF_LIBRARY} ${CURL_LIBRARY} ${OPENGL_LIBRARY} ${COCOA_LIBRARY} ${OPENSSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC "${XPLANE_INCLUDES_PATH}/XPLM" "${XPLANE_INCLUDES_PATH}/Widgets" "${XPLANE_INCLUDES_PATH}/Wrappers")
    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC "${CEF_ROOT}")
    HEADER_DIRECTORIES(header_dir_list ${src_dir})
    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC ${header_dir_list})
    # Expose src/ itself so consumers can #include "skyscript_c.h"
    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC ${src_dir})
    use_plugin_local_runtime_paths(${library_name})
ENDFUNCTION()

# Derive version from git tags
EXECUTE_PROCESS(
    COMMAND git describe --tags --match "v*" --exact-match
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_TAG_RESULT
)

IF(GIT_TAG_RESULT EQUAL 0)
    # On an exact tag like "v1.0.6" -> "1.0.6"
    STRING(REGEX REPLACE "^v" "" GIT_VERSION "${GIT_TAG}")
ELSE()
    # Not on an exact tag: use latest tag + "-dev"
    EXECUTE_PROCESS(
        COMMAND git describe --tags --match "v*" --abbrev=0
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_LATEST_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_LATEST_TAG_RESULT
    )
    IF(GIT_LATEST_TAG_RESULT EQUAL 0)
        STRING(REGEX REPLACE "^v" "" GIT_VERSION "${GIT_LATEST_TAG}")
        SET(GIT_VERSION "${GIT_VERSION}-dev")
    ELSE()
        SET(GIT_VERSION "0.0.0-dev")
    ENDIF()
ENDIF()

MESSAGE(STATUS "SkyScript version: ${GIT_VERSION}")

# Build SkyScript as a shared library
add_skyscript_library(SkyScriptLib 420 "${CMAKE_CURRENT_SOURCE_DIR}/src")
TARGET_COMPILE_DEFINITIONS(SkyScriptLib PUBLIC SKYSCRIPT_VERSION="${GIT_VERSION}")

# Build the example plugin that uses the SkyScript shared library via C API
add_xplane_plugin(SkyScript 420 "${CMAKE_CURRENT_SOURCE_DIR}/example")
TARGET_LINK_LIBRARIES(SkyScript PUBLIC SkyScriptLib)
TARGET_COMPILE_DEFINITIONS(SkyScript PRIVATE SKYSCRIPT_VERSION="${GIT_VERSION}")
