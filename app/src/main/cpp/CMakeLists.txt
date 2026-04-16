cmake_minimum_required(VERSION 3.22.1)
project(fireplay LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(LIB_UXPLAY ${CMAKE_SOURCE_DIR}/../../../../lib-uxplay)
set(KDAB_OPENSSL ${CMAKE_SOURCE_DIR}/../../../../third_party/android_openssl)
set(LIBPLIST ${CMAKE_SOURCE_DIR}/../../../../third_party/libplist)
set(ALAC ${CMAKE_SOURCE_DIR}/../../../../third_party/alac)

# ALAC decoder (Apple official). Compile decoder + bit/endian/codec helpers.
add_library(alac_dec STATIC
    ${ALAC}/codec/ALACDecoder.cpp
    ${ALAC}/codec/ALACBitUtilities.c
    ${ALAC}/codec/EndianPortable.c
    ${ALAC}/codec/ag_dec.c
    ${ALAC}/codec/dp_dec.c
    ${ALAC}/codec/matrix_dec.c
)
target_include_directories(alac_dec PUBLIC ${ALAC}/codec)
target_compile_definitions(alac_dec PRIVATE -DTARGET_OS_GENERIC=1 -DTARGET_RT_LITTLE_ENDIAN=1)

# libplist (vendored upstream src; pure C, no autotools)
file(GLOB LIBPLIST_C ${LIBPLIST}/src/*.c)
file(GLOB LIBCNARY_C ${LIBPLIST}/libcnary/*.c)
add_library(plist STATIC ${LIBPLIST_C} ${LIBCNARY_C})
target_include_directories(plist PUBLIC
    ${LIBPLIST}/include
    ${LIBPLIST}/src
    ${LIBPLIST}/libcnary/include
)
target_include_directories(plist PRIVATE ${LIBPLIST})
target_compile_definitions(plist PRIVATE -DHAVE_CONFIG_H=1 -D_GNU_SOURCE)

# Pre-built KDAB OpenSSL (per-ABI)
add_library(crypto_static STATIC IMPORTED)
set_target_properties(crypto_static PROPERTIES
    IMPORTED_LOCATION ${KDAB_OPENSSL}/ssl_3/${CMAKE_ANDROID_ARCH_ABI}/libcrypto.a)

# llhttp parser (vendored in UxPlay tree)
file(GLOB LLHTTP_SRC ${LIB_UXPLAY}/llhttp/*.c)
add_library(llhttp STATIC ${LLHTTP_SRC})
target_include_directories(llhttp PUBLIC ${LIB_UXPLAY}/llhttp)

# playfair (FairPlay handshake emulation)
file(GLOB PLAYFAIR_SRC ${LIB_UXPLAY}/playfair/*.c)
add_library(playfair STATIC ${PLAYFAIR_SRC})
target_include_directories(playfair PUBLIC ${LIB_UXPLAY}/playfair)
target_include_directories(playfair PRIVATE ${KDAB_OPENSSL}/ssl_3/include)

# UxPlay protocol library — all lib/*.c except dnssd.c (replaced by stub).
file(GLOB UXPLAY_SRC ${LIB_UXPLAY}/*.c)
list(REMOVE_ITEM UXPLAY_SRC ${LIB_UXPLAY}/dnssd.c)
list(REMOVE_ITEM UXPLAY_SRC ${LIB_UXPLAY}/fairplay_playfair.c)
add_library(airplay STATIC
    ${UXPLAY_SRC}
    ${LIB_UXPLAY}/fairplay_playfair.c
    dnssd_stub.c
)
target_include_directories(airplay PUBLIC
    ${LIB_UXPLAY}
    ${LIB_UXPLAY}/llhttp
    ${LIB_UXPLAY}/playfair
    ${KDAB_OPENSSL}/ssl_3/include
)
target_compile_definitions(airplay PRIVATE
    -DSTANDALONE
    -D__STDC_CONSTANT_MACROS
    -D__STDC_LIMIT_MACROS
    -D_REENTRANT
    -D_LARGEFILE64_SOURCE
    -D_FILE_OFFSET_BITS=64
    -DNOHOLD
    -DOPENSSL_API_COMPAT=0x10101000L
    -DPLIST_210
    -DPLIST_230
)
target_link_libraries(airplay PUBLIC llhttp playfair plist crypto_static)

# JNI bridge → libfireplay.so (loaded by Java System.loadLibrary)
add_library(fireplay SHARED
    fireplay_jni.cpp
    audio_renderer_aaudio.cpp
    video_renderer_mediacodec.cpp
)
target_include_directories(fireplay PRIVATE ${ALAC}/codec)
find_library(log-lib log)
find_library(aaudio-lib aaudio)
find_library(android-lib android)
find_library(mediandk-lib mediandk)
target_link_libraries(fireplay airplay alac_dec
    ${log-lib} ${aaudio-lib} ${android-lib} ${mediandk-lib})
