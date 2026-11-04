# Keep application libraries private: InkView loads its own firmware copies.
# Sources are pinned in thirdparty/versions.json; the build never downloads code.
set(SKIP_INSTALL_ALL ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build private static dependencies" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/zlib)
add_library(ZLIB::ZLIB ALIAS zlibstatic)
set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/zlib CACHE PATH "" FORCE)
set(ZLIB_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/thirdparty/zlib ${ZLIB_INCLUDE_DIR})
set(ZLIB_LIBRARY_RELEASE zlibstatic CACHE STRING "" FORCE)
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_LIBRARIES zlibstatic)

set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/libpng)
add_library(PNG::PNG ALIAS png_static)
set(PNG_PNG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/libpng CACHE PATH "" FORCE)
set(PNG_INCLUDE_DIR ${CMAKE_BINARY_DIR}/thirdparty/libpng ${PNG_PNG_INCLUDE_DIR})
set(PNG_LIBRARY_RELEASE png_static CACHE STRING "" FORCE)
set(PNG_LIBRARY png_static)
set(PNG_LIBRARIES png_static)

# libjpeg-turbo explicitly requires a separate CMake project.
include(ExternalProject)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/thirdparty/libjpeg)
ExternalProject_Add(jpeg_external
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/libjpeg
  BINARY_DIR ${CMAKE_BINARY_DIR}/thirdparty/libjpeg
  CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
    -DENABLE_SHARED=OFF -DENABLE_STATIC=ON
    -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/thirdparty/libjpeg/libjpeg.a)
add_library(jpeg STATIC IMPORTED GLOBAL)
set_target_properties(jpeg PROPERTIES
  IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/thirdparty/libjpeg/libjpeg.a)
add_dependencies(jpeg jpeg_external)
set(JPEG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/libjpeg/src ${CMAKE_BINARY_DIR}/thirdparty/libjpeg)
set(JPEG_LIBRARIES jpeg)

set(FT_REQUIRE_PNG ON CACHE BOOL "" FORCE)
set(FT_REQUIRE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HVF ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/freetype)
set(FREETYPE_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/thirdparty/freetype/include)
set(FREETYPE_LIBRARIES freetype)

# Isolate curl's configure checks from the in-tree zlib/libpng targets.
# Only HTTP(S) is used; OpenSSL remains supplied by the compatible firmware SDK.
set(curl_args)
foreach(protocol DICT FILE FTP GOPHER IMAP LDAP LDAPS MQTT POP3 RTSP SMTP TELNET TFTP IPFS WEBSOCKETS)
  list(APPEND curl_args -DCURL_DISABLE_${protocol}=ON)
endforeach()
ExternalProject_Add(curl_external
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/curl
  BINARY_DIR ${CMAKE_BINARY_DIR}/thirdparty/curl
  DEPENDS zlibstatic
  CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
    -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON -DBUILD_CURL_EXE=OFF
    -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF
    -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF
    -DCURL_DISABLE_INSTALL=ON -DCURL_USE_PKGCONFIG=OFF
    -DCURL_USE_OPENSSL=ON -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF
    -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_LIBIDN2=OFF -DUSE_NGHTTP2=OFF
    -DZLIB_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/thirdparty/zlib
    -DZLIB_LIBRARY=$<TARGET_FILE:zlibstatic>
    # Firmware OpenSSL 3 builds may omit the deprecated ENGINE API while still
    # exporting OPENSSL_3.0.0. CoolReader uses normal HTTPS, never legacy engines.
    "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS} -I${CMAKE_BINARY_DIR}/thirdparty/zlib -DOPENSSL_NO_ENGINE"
    -DCURL_CA_BUNDLE=/ebrmain/share/ssl/certs/ca-certificates.crt
    -DCURL_CA_PATH=none
    ${curl_args}
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/thirdparty/curl/lib/libcurl.a)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
add_library(libcurl_static STATIC IMPORTED GLOBAL)
set_target_properties(libcurl_static PROPERTIES
  IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/thirdparty/curl/lib/libcurl.a
  INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto;Threads::Threads;zlibstatic")
add_dependencies(libcurl_static curl_external)
set(CURL_LIBRARIES libcurl_static)
set(CURL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/thirdparty/curl/include)

add_subdirectory(thirdparty/qimagescale)

# The independent regression programs use the same link inputs as the app.
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/dependency-link.json CONTENT
  "{\"archives\":[\"$<TARGET_FILE:jpeg>\",\"$<TARGET_FILE:freetype>\",\"$<TARGET_FILE:png_static>\",\"$<TARGET_FILE:zlibstatic>\",\"$<TARGET_FILE:libcurl_static>\",\"$<TARGET_FILE:qimagescale>\"],\"runtime\":[\"ssl\",\"crypto\",\"bz2\",\"pthread\",\"dl\",\"m\"]}\n")
