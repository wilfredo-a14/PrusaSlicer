set(_jpeg_simd_args)
set(_jpeg_runtime_args)
if(MSVC)
    # Match the /MD runtime used by PrusaSlicer, wxWidgets, and the other
    # dependency projects instead of libjpeg-turbo's static-CRT default.
    list(APPEND _jpeg_runtime_args -DWITH_CRT_DLL=ON)
endif()
if(MSVC AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]64$" AND
   CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    # libjpeg-turbo 3.0.1 mistakes the ARM64 host for the x64 target when
    # Visual Studio is cross-compiling on Windows on ARM.
    list(APPEND _jpeg_simd_args -DWITH_SIMD=OFF)
endif()

add_cmake_project(JPEG
    URL https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.1.zip
    URL_HASH SHA256=d6d99e693366bc03897677650e8b2dfa76b5d6c54e2c9e70c03f0af821b0a52f
    CMAKE_ARGS
        -DENABLE_SHARED=OFF
        -DENABLE_STATIC=ON
        ${_jpeg_runtime_args}
        ${_jpeg_simd_args}
)

set(DEP_JPEG_DEPENDS ZLIB)
