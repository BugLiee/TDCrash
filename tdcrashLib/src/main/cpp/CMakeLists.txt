cmake_minimum_required(VERSION 3.10.2)

#######################################
# global
#######################################

add_compile_options(
        -std=c11
        -Weverything
        -Werror)

#######################################
# libtdcrash.so
#######################################

file(GLOB TDCRASH_SRC
        tdcrash/*.c
        common/*.c
        dl/*.c)

add_library(tdcrash SHARED
        ${TDCRASH_SRC})

target_include_directories(tdcrash PUBLIC
        tdcrash
        tdcrash_dumper
        common
        dl)

target_link_libraries(tdcrash
        log
        dl)

if(USEASAN)

    target_compile_options(tdcrash PUBLIC
            -fsanitize=address
            -fno-omit-frame-pointer)

    set_target_properties(tdcrash PROPERTIES
            LINK_FLAGS " \
        -fsanitize=address")

else()

    target_compile_options(tdcrash PUBLIC
            -Oz
            -flto
            -ffunction-sections
            -fdata-sections)

    set_target_properties(tdcrash PROPERTIES
            LINK_FLAGS " \
        -O3 \
        -flto \
        -Wl,--exclude-libs,ALL \
        -Wl,--gc-sections \
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/tdcrash.exports")

endif()

#######################################
# libtdcrash_dumper.so
#######################################

file(GLOB TDCRASH_DUMPER_SRC
        tdcrash_dumper/*.c
        common/*.c)

set(LZME_SRC
        lzma/7zCrc.c
        lzma/7zCrcOpt.c
        lzma/CpuArch.c
        lzma/Bra.c
        lzma/Bra86.c
        lzma/BraIA64.c
        lzma/Delta.c
        lzma/Lzma2Dec.c
        lzma/LzmaDec.c
        lzma/Sha256.c
        lzma/Xz.c
        lzma/XzCrc64.c
        lzma/XzCrc64Opt.c
        lzma/XzDec.c)

set_source_files_properties(${LZME_SRC} PROPERTIES
        COMPILE_FLAGS " \
        -D_7ZIP_ST \
        -Wno-enum-conversion \
        -Wno-reserved-id-macro \
        -Wno-undef \
        -Wno-missing-prototypes \
        -Wno-missing-variable-declarations \
        -Wno-cast-align \
        -Wno-sign-conversion \
        -Wno-assign-enum \
        -Wno-unused-macros \
        -Wno-padded \
        -Wno-cast-qual \
        -Wno-strict-prototypes \
        -Wno-extra-semi-stmt")

add_executable(tdcrash_dumper
        ${TDCRASH_DUMPER_SRC}
        ${LZME_SRC})

target_include_directories(tdcrash_dumper PUBLIC
        tdcrash_dumper
        common
        lzma)

target_link_libraries(tdcrash_dumper
        log
        dl)

if(USEASAN)

    target_compile_options(tdcrash_dumper PUBLIC
            -fsanitize=address
            -fno-omit-frame-pointer)

    set_target_properties(tdcrash_dumper PROPERTIES
            LINK_FLAGS " \
        -fsanitize=address")

else()

    target_compile_options(tdcrash_dumper PUBLIC
            -Oz
            -flto
            -ffunction-sections
            -fdata-sections)

    set_target_properties(tdcrash_dumper PROPERTIES
            LINK_FLAGS " \
        -O3 \
        -flto \
        -Wl,--exclude-libs,ALL \
        -Wl,--gc-sections")

endif()

set_target_properties(tdcrash_dumper PROPERTIES
        PREFIX "lib"
        SUFFIX ".so")
