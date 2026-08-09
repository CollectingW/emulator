# SPDX-FileCopyrightText: 2026 citron Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

include_guard(GLOBAL)

function(citron_build_clangtron_ffmpeg)
    if(NOT DEFINED FFMPEG_CPM_SOURCE_DIR OR
       NOT IS_DIRECTORY "${FFMPEG_CPM_SOURCE_DIR}")
        message(FATAL_ERROR "clangtron build requires CPM FFmpeg source")
    endif()

    set(CITRON_MSYS2_ROOT "" CACHE PATH "MSYS2 install root (auto-detected if empty)")
    if (CITRON_MSYS2_ROOT)
        find_program(BASH_PROGRAM bash
            HINTS "${CITRON_MSYS2_ROOT}/usr/bin" REQUIRED)
        find_program(MAKE_PROGRAM make
            HINTS "${CITRON_MSYS2_ROOT}/usr/bin" REQUIRED)
    else()
        find_program(BASH_PROGRAM bash REQUIRED)
        find_program(MAKE_PROGRAM make REQUIRED)
    endif()
    include(ProcessorCount)
    ProcessorCount(_ffmpeg_jobs)
    if(NOT _ffmpeg_jobs)
        set(_ffmpeg_jobs 4)
    endif()

    set(_source_dir "${FFMPEG_CPM_SOURCE_DIR}")
    if (DEFINED CLANGTRON_FFMPEG_CACHE_DIR AND NOT "${CLANGTRON_FFMPEG_CACHE_DIR}" STREQUAL "")
        set(_build_dir  "${CLANGTRON_FFMPEG_CACHE_DIR}/build")
        set(_install_dir "${CLANGTRON_FFMPEG_CACHE_DIR}/install")
        message(STATUS "[FFmpeg/clangtron] Using global cache dir: ${CLANGTRON_FFMPEG_CACHE_DIR}")
    else()
        set(_build_dir  "${PROJECT_BINARY_DIR}/externals/ffmpeg-clangtron-build")
        set(_install_dir "${PROJECT_BINARY_DIR}/externals/ffmpeg-clangtron-install")
    endif()
    
    get_filename_component(_clangtron_tool_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    
    if(CMAKE_HOST_WIN32)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "MSYS2_ARG_CONV_EXCL=*"
                "${BASH_PROGRAM}" -lc "cygpath -am '${_source_dir}' && cygpath -am '${_build_dir}' && cygpath -am '${_install_dir}' && cygpath -au '${_clangtron_tool_dir}' && cygpath -am '${CMAKE_C_COMPILER}' && cygpath -am '${CMAKE_RC_COMPILER}'"
            OUTPUT_VARIABLE _clangtron_ffmpeg_paths
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
        string(REPLACE "\n" ";" _clangtron_ffmpeg_paths "${_clangtron_ffmpeg_paths}")
        list(GET _clangtron_ffmpeg_paths 0 _source_dir_win)
        list(GET _clangtron_ffmpeg_paths 1 _build_dir_win)
        list(GET _clangtron_ffmpeg_paths 2 _install_dir_win)
        list(GET _clangtron_ffmpeg_paths 3 _clangtron_tool_dir_msys)
        list(GET _clangtron_ffmpeg_paths 4 _c_compiler_win)
        list(GET _clangtron_ffmpeg_paths 5 _rc_compiler_win)

        # MSYS paths for bash commands (cd, mv) — separate from Windows mixed paths
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "MSYS2_ARG_CONV_EXCL=*"
                "${BASH_PROGRAM}" -lc "cygpath -au '${_install_dir}'"
            OUTPUT_VARIABLE _install_dir_msys
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
    else()
        set(_source_dir_win "${_source_dir}")
        set(_build_dir_win "${_build_dir}")
        set(_install_dir_win "${_install_dir}")
        set(_clangtron_tool_dir_msys "${_clangtron_tool_dir}")
        set(_c_compiler_win "${CMAKE_C_COMPILER}")
        set(_rc_compiler_win "${CMAKE_RC_COMPILER}")
        set(_install_dir_msys "${_install_dir}")
    endif()

    set(_build_stamp "${_install_dir}/.built")
    file(MAKE_DIRECTORY "${_build_dir}" "${_install_dir}")

    set(_ffmpeg_extra_cflags "")
    if (Vulkan-Headers_SOURCE_DIR)
        if(CMAKE_HOST_WIN32)
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E env "MSYS2_ARG_CONV_EXCL=*"
                    "${BASH_PROGRAM}" -lc "cygpath -am '${Vulkan-Headers_SOURCE_DIR}'"
                OUTPUT_VARIABLE _vk_headers_win
                OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY
            )
        else()
            set(_vk_headers_win "${Vulkan-Headers_SOURCE_DIR}")
        endif()
        set(_ffmpeg_extra_cflags "${_ffmpeg_extra_cflags} -I${_vk_headers_win}/include")
    endif()

    if (DEFINED CLANGTRON_FFMPEG_EXTRA_CFLAGS AND NOT "${CLANGTRON_FFMPEG_EXTRA_CFLAGS}" STREQUAL "")
        set(_ffmpeg_extra_cflags "${_ffmpeg_extra_cflags} ${CLANGTRON_FFMPEG_EXTRA_CFLAGS}")
    endif()

    # Flag sentinel: if cached build's flags differ from current, remove stamp so ninja rebuilds.
    set(_ffmpeg_flags_sentinel "${_install_dir}/.citron-clangtron-extra-cflags")
    set(_ffmpeg_flags_sentinel_content "")
    if (EXISTS "${_ffmpeg_flags_sentinel}")
        file(READ "${_ffmpeg_flags_sentinel}" _ffmpeg_flags_sentinel_content)
        string(STRIP "${_ffmpeg_flags_sentinel_content}" _ffmpeg_flags_sentinel_content)
    endif()
    if (EXISTS "${_build_stamp}" AND NOT _ffmpeg_flags_sentinel_content STREQUAL "${_ffmpeg_extra_cflags}")
        message(STATUS "[FFmpeg/clangtron] Cached build's recorded flags don't match the current build's; rebuilding")
        file(REMOVE "${_build_stamp}")
    endif()
    file(WRITE "${_ffmpeg_flags_sentinel}" "${_ffmpeg_extra_cflags}")

    set(_ffmpeg_configure_command
        "export PATH='${_clangtron_tool_dir_msys}':$PATH &&"
        "'${_source_dir_win}/configure'"
        "--arch=x86_64"
        "--target-os=mingw32"
        "--cc='${_c_compiler_win}'"
        "--ar=llvm-ar"
        "--nm=llvm-nm"
        "--strip=llvm-strip"
        "--ranlib=llvm-ranlib"
        "--windres='${_rc_compiler_win}'"
        "--prefix='${_install_dir_win}'"
        "--disable-pthreads"
        "--enable-w32threads"
        "--enable-static"
        "--disable-shared"
        "--disable-doc"
        "--disable-programs"
        "--disable-avdevice"
        "--disable-network"
        "--disable-everything"
        "--disable-vaapi"
        "--disable-vdpau"
        "--disable-iconv"
        "--enable-decoder=h264,vp8,vp9,aac,mp3,opus,flac"
        "--enable-demuxer=mp4,matroska,ogg"
        "--enable-filter=yadif,scale,aresample"
        "--enable-protocol=file"
        "--enable-dxva2"
        "--enable-d3d11va"
    )

    if(NOT CMAKE_HOST_WIN32)
        list(APPEND _ffmpeg_configure_command "--enable-cross-compile" "--cross-prefix=${_clangtron_tool_dir_msys}/x86_64-w64-mingw32-")
    else()
        list(APPEND _ffmpeg_configure_command "--host-cc='${_c_compiler_win}'")
    endif()

    if(NOT "${_ffmpeg_extra_cflags}" STREQUAL "")
        list(APPEND _ffmpeg_configure_command "--extra-cflags='${_ffmpeg_extra_cflags}'")
    endif()

    string(JOIN " " _ffmpeg_configure_command ${_ffmpeg_configure_command})

    add_custom_command(
        OUTPUT "${_build_stamp}"
        BYPRODUCTS
            "${_install_dir}/lib/libavfilter.a"
            "${_install_dir}/lib/libswscale.a"
            "${_install_dir}/lib/libswresample.a"
            "${_install_dir}/lib/libavcodec.a"
            "${_install_dir}/lib/libavutil.a"
            "${_install_dir}/lib/libavformat.a"
        COMMAND "${BASH_PROGRAM}" -lc "${_ffmpeg_configure_command}"
        COMMAND "${BASH_PROGRAM}" -lc "export PATH='${_clangtron_tool_dir_msys}':$PATH && '${MAKE_PROGRAM}' -j${_ffmpeg_jobs}"
        COMMAND "${BASH_PROGRAM}" -lc "export PATH='${_clangtron_tool_dir_msys}':$PATH && '${MAKE_PROGRAM}' install"
        COMMAND "${CMAKE_COMMAND}" -E touch "${_build_stamp}"
        DEPENDS "${CMAKE_CURRENT_LIST_FILE}" "${_source_dir}/configure"
        WORKING_DIRECTORY "${_build_dir_win}"
        VERBATIM
    )
    add_custom_target(ffmpeg-build ALL DEPENDS "${_build_stamp}")

    set(CLANGTRON_FFMPEG_BUILD_STAMP "${_build_stamp}" CACHE INTERNAL
        "Stamp file written when clangtron FFmpeg build+install completes")

    set(_libraries
        "${_install_dir}/lib/libavfilter.a"
        "${_install_dir}/lib/libswscale.a"
        "${_install_dir}/lib/libswresample.a"
        "${_install_dir}/lib/libavformat.a"
        "${_install_dir}/lib/libavcodec.a"
        "${_install_dir}/lib/libavutil.a"
        bcrypt ole32 strmiids mfuuid mfplat uuid d3d11 dxgi dxva2)

    set(FFmpeg_FOUND YES CACHE BOOL "" FORCE)
    set(FFmpeg_INCLUDE_DIR "${_install_dir}/include"
        CACHE PATH "Path to clangtron FFmpeg headers" FORCE)
    set(FFmpeg_LIBRARIES "${_libraries}"
        CACHE STRING "clangtron FFmpeg libraries" FORCE)
    set(FFmpeg_LDFLAGS "" CACHE STRING "FFmpeg linker flags" FORCE)
    set(FFmpeg_FOUND YES PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${_install_dir}/include" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${_libraries}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "" PARENT_SCOPE)
endfunction()
