cmake_minimum_required(VERSION 3.20)

project(dj1000 VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DJ1000_BUILD_CLI "Build the standalone dj1000 CLI" ON)
option(DJ1000_BUILD_TESTS "Build the native test suite" ON)
option(DJ1000_INSTALL "Enable install/export packaging targets" ON)
option(DJ1000_BUILD_WASM "Build the standalone WebAssembly module" OFF)
option(DJ1000_ENABLE_ADOBE_DNG_SDK "Build the Adobe DNG SDK-backed modern RAW exporter when the SDK is available" ON)

set(
    DJ1000_ADOBE_DNG_SDK_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../dng_sdk_1_7_1"
    CACHE PATH
    "Path to the Adobe DNG SDK root directory"
)

set(DJ1000_HAVE_ADOBE_DNG_SDK OFF)
if(
    DJ1000_ENABLE_ADOBE_DNG_SDK
    AND EXISTS "${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source/dng_negative.h"
)
    set(DJ1000_HAVE_ADOBE_DNG_SDK ON)

    file(
        GLOB DJ1000_ADOBE_DNG_SDK_SOURCES
        CONFIGURE_DEPENDS
        "${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source/*.cpp"
    )
    list(
        REMOVE_ITEM
        DJ1000_ADOBE_DNG_SDK_SOURCES
        "${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source/dng_validate.cpp"
        "${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source/dng_jxl.cpp"
        "${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source/dng_update_meta.cpp"
        "${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source/dng_xmp_sdk.cpp"
    )

    add_library(dj1000_adobe_dng_sdk STATIC
        ${DJ1000_ADOBE_DNG_SDK_SOURCES}
        src/adobe_dng_sdk_jxl_stub.cpp
    )
    set(DJ1000_LIBJPEG_ROOT "${DJ1000_ADOBE_DNG_SDK_ROOT}/libjpeg")
    add_library(dj1000_libjpeg STATIC
        "${DJ1000_LIBJPEG_ROOT}/jcapimin.c"
        "${DJ1000_LIBJPEG_ROOT}/jcapistd.c"
        "${DJ1000_LIBJPEG_ROOT}/jdapimin.c"
        "${DJ1000_LIBJPEG_ROOT}/jdapistd.c"
        "${DJ1000_LIBJPEG_ROOT}/jcomapi.c"
        "${DJ1000_LIBJPEG_ROOT}/jcparam.c"
        "${DJ1000_LIBJPEG_ROOT}/jctrans.c"
        "${DJ1000_LIBJPEG_ROOT}/jdtrans.c"
        "${DJ1000_LIBJPEG_ROOT}/jcinit.c"
        "${DJ1000_LIBJPEG_ROOT}/jcmaster.c"
        "${DJ1000_LIBJPEG_ROOT}/jcmainct.c"
        "${DJ1000_LIBJPEG_ROOT}/jcprepct.c"
        "${DJ1000_LIBJPEG_ROOT}/jccoefct.c"
        "${DJ1000_LIBJPEG_ROOT}/jccolor.c"
        "${DJ1000_LIBJPEG_ROOT}/jcsample.c"
        "${DJ1000_LIBJPEG_ROOT}/jcdctmgr.c"
        "${DJ1000_LIBJPEG_ROOT}/jfdctint.c"
        "${DJ1000_LIBJPEG_ROOT}/jfdctfst.c"
        "${DJ1000_LIBJPEG_ROOT}/jfdctflt.c"
        "${DJ1000_LIBJPEG_ROOT}/jchuff.c"
        "${DJ1000_LIBJPEG_ROOT}/jcarith.c"
        "${DJ1000_LIBJPEG_ROOT}/jcmarker.c"
        "${DJ1000_LIBJPEG_ROOT}/jdatadst.c"
        "${DJ1000_LIBJPEG_ROOT}/jdmaster.c"
        "${DJ1000_LIBJPEG_ROOT}/jdinput.c"
        "${DJ1000_LIBJPEG_ROOT}/jdmainct.c"
        "${DJ1000_LIBJPEG_ROOT}/jdcoefct.c"
        "${DJ1000_LIBJPEG_ROOT}/jdpostct.c"
        "${DJ1000_LIBJPEG_ROOT}/jdmarker.c"
        "${DJ1000_LIBJPEG_ROOT}/jdhuff.c"
        "${DJ1000_LIBJPEG_ROOT}/jdarith.c"
        "${DJ1000_LIBJPEG_ROOT}/jddctmgr.c"
        "${DJ1000_LIBJPEG_ROOT}/jidctint.c"
        "${DJ1000_LIBJPEG_ROOT}/jidctfst.c"
        "${DJ1000_LIBJPEG_ROOT}/jidctflt.c"
        "${DJ1000_LIBJPEG_ROOT}/jdsample.c"
        "${DJ1000_LIBJPEG_ROOT}/jdcolor.c"
        "${DJ1000_LIBJPEG_ROOT}/jdmerge.c"
        "${DJ1000_LIBJPEG_ROOT}/jquant1.c"
        "${DJ1000_LIBJPEG_ROOT}/jquant2.c"
        "${DJ1000_LIBJPEG_ROOT}/jdatasrc.c"
        "${DJ1000_LIBJPEG_ROOT}/jaricom.c"
        "${DJ1000_LIBJPEG_ROOT}/jerror.c"
        "${DJ1000_LIBJPEG_ROOT}/jmemmgr.c"
        "${DJ1000_LIBJPEG_ROOT}/jutils.c"
        "${DJ1000_LIBJPEG_ROOT}/jmemnobs.c"
    )
    set_target_properties(dj1000_libjpeg PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(dj1000_libjpeg SYSTEM PRIVATE
        ${DJ1000_LIBJPEG_ROOT}
    )
    target_compile_options(dj1000_libjpeg PRIVATE
        $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-w>
    )
    set_target_properties(dj1000_adobe_dng_sdk PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(dj1000_adobe_dng_sdk
        SYSTEM PRIVATE
            ${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/adobe_dng_sdk_stubs
            ${DJ1000_LIBJPEG_ROOT}
    )

    set(DJ1000_ADOBE_DNG_SDK_PLATFORM_DEFINES)
    if(EMSCRIPTEN)
        list(APPEND DJ1000_ADOBE_DNG_SDK_PLATFORM_DEFINES qMacOS=0 qWinOS=0 qLinux=0 qWeb=1)
    elseif(APPLE)
        list(APPEND DJ1000_ADOBE_DNG_SDK_PLATFORM_DEFINES qMacOS=1 qWinOS=0 qLinux=0 qWeb=0)
    elseif(WIN32)
        list(APPEND DJ1000_ADOBE_DNG_SDK_PLATFORM_DEFINES qMacOS=0 qWinOS=1 qLinux=0 qWeb=0)
    else()
        list(APPEND DJ1000_ADOBE_DNG_SDK_PLATFORM_DEFINES qMacOS=0 qWinOS=0 qLinux=1 qWeb=0)
    endif()

    if(NOT EMSCRIPTEN)
        find_package(ZLIB REQUIRED)
        find_package(Threads REQUIRED)
    endif()
    target_compile_definitions(dj1000_adobe_dng_sdk PRIVATE
        ${DJ1000_ADOBE_DNG_SDK_PLATFORM_DEFINES}
        qDNGUseXMP=0
        qDNGUseLibJPEG=1
        qDNGValidate=0
        qDNGValidateTarget=0
        qDNGThreadSafe=$<IF:$<BOOL:${EMSCRIPTEN}>,0,1>
    )
    if(EMSCRIPTEN)
        target_compile_options(dj1000_adobe_dng_sdk PRIVATE -sUSE_ZLIB=1)
    else()
        target_link_libraries(dj1000_adobe_dng_sdk PRIVATE
            Threads::Threads
            ZLIB::ZLIB
        )
    endif()
    target_link_libraries(dj1000_adobe_dng_sdk PRIVATE dj1000_libjpeg)
    if(APPLE)
        find_library(DJ1000_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
        find_library(DJ1000_CARBON_FRAMEWORK Carbon REQUIRED)
        target_link_libraries(dj1000_adobe_dng_sdk PUBLIC
            ${DJ1000_COREFOUNDATION_FRAMEWORK}
            ${DJ1000_CARBON_FRAMEWORK}
        )
    endif()
    target_compile_options(dj1000_adobe_dng_sdk PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-w>
    )
endif()

add_library(dj1000_native_lib
    src/bmp.cpp
    src/bright_vertical_gate.cpp
    src/c_api.cpp
    src/converter.cpp
    src/converter_io.cpp
    src/dat_file.cpp
    src/dat_file_io.cpp
    src/geometry_copy.cpp
    src/index_view.cpp
    src/large_export_pipeline.cpp
    src/large_vertical_resample.cpp
    src/modern_export_pipeline.cpp
    src/modern_dng.cpp
    src/modern_dng_sdk.cpp
    src/modern_raw_frame.cpp
    src/modern_sensor_frame.cpp
    src/nonlarge_geometry.cpp
    src/nonlarge_post_geometry_pipeline.cpp
    src/nonlarge_source_pipeline.cpp
    src/normal_export_pipeline.cpp
    src/nonlarge_row_resample.cpp
    src/post_geometry_center_scale.cpp
    src/post_geometry_dual_scale.cpp
    src/post_geometry_edge_response.cpp
    src/post_geometry_filter.cpp
    src/post_geometry_prepare.cpp
    src/post_geometry_rgb_convert.cpp
    src/post_geometry_stage_3060.cpp
    src/post_geometry_stage_3600.cpp
    src/post_geometry_stage_3890.cpp
    src/post_geometry_stage_2a00.cpp
    src/post_geometry_stage_2dd0.cpp
    src/post_geometry_stage_4810.cpp
    src/post_rgb_stage_40f0.cpp
    src/post_rgb_stage_42a0.cpp
    src/post_rgb_stage_3b00.cpp
    src/pregeometry_pipeline.cpp
    src/pregeometry_normalize.cpp
    src/quantize_stage.cpp
    src/resample_lut.cpp
    src/source_center_stage.cpp
    src/source_stage.cpp
    src/source_seed_stage.cpp
)
add_library(dj1000::dj1000 ALIAS dj1000_native_lib)
set_target_properties(dj1000_native_lib PROPERTIES
    EXPORT_NAME dj1000
    OUTPUT_NAME dj1000
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(dj1000_native_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_options(dj1000_native_lib PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)
if(EMSCRIPTEN)
    target_compile_options(dj1000_native_lib PRIVATE -fexceptions)
endif()
if(DJ1000_HAVE_ADOBE_DNG_SDK)
    target_link_libraries(dj1000_native_lib PRIVATE dj1000_adobe_dng_sdk)
    target_include_directories(dj1000_native_lib
        SYSTEM PRIVATE
            ${DJ1000_ADOBE_DNG_SDK_ROOT}/dng_sdk/source
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/adobe_dng_sdk_stubs
    )
    target_compile_definitions(dj1000_native_lib PRIVATE
        DJ1000_HAVE_ADOBE_DNG_SDK=1
        ${DJ1000_ADOBE_DNG_SDK_PLATFORM_DEFINES}
        qDNGUseXMP=0
        qDNGUseLibJPEG=1
        qDNGValidate=0
        qDNGValidateTarget=0
        qDNGThreadSafe=$<IF:$<BOOL:${EMSCRIPTEN}>,0,1>
    )
else()
    target_compile_definitions(dj1000_native_lib PRIVATE DJ1000_HAVE_ADOBE_DNG_SDK=0)
endif()

if(DJ1000_BUILD_CLI)
    add_executable(dj1000
        src/main.cpp
    )
    target_link_libraries(dj1000 PRIVATE dj1000_native_lib)
endif()

if(DJ1000_BUILD_TESTS)
    add_executable(test_index_view
        tests/test_index_view.cpp
    )
    target_link_libraries(test_index_view PRIVATE dj1000_native_lib)

    add_executable(test_resample_lut
        tests/test_resample_lut.cpp
    )
    target_link_libraries(test_resample_lut PRIVATE dj1000_native_lib)

    add_executable(test_geometry_copy
        tests/test_geometry_copy.cpp
    )
    target_link_libraries(test_geometry_copy PRIVATE dj1000_native_lib)

    add_executable(test_large_vertical_resample
        tests/test_large_vertical_resample.cpp
    )
    target_link_libraries(test_large_vertical_resample PRIVATE dj1000_native_lib)

    add_executable(test_nonlarge_row_resample
        tests/test_nonlarge_row_resample.cpp
    )
    target_link_libraries(test_nonlarge_row_resample PRIVATE dj1000_native_lib)

    add_executable(test_nonlarge_geometry
        tests/test_nonlarge_geometry.cpp
    )
    target_link_libraries(test_nonlarge_geometry PRIVATE dj1000_native_lib)

    add_executable(test_nonlarge_source_pipeline
        tests/test_nonlarge_source_pipeline.cpp
    )
    target_link_libraries(test_nonlarge_source_pipeline PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_prepare
        tests/test_post_geometry_prepare.cpp
    )
    target_link_libraries(test_post_geometry_prepare PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_center_scale
        tests/test_post_geometry_center_scale.cpp
    )
    target_link_libraries(test_post_geometry_center_scale PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_dual_scale
        tests/test_post_geometry_dual_scale.cpp
    )
    target_link_libraries(test_post_geometry_dual_scale PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_edge_response
        tests/test_post_geometry_edge_response.cpp
    )
    target_link_libraries(test_post_geometry_edge_response PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_filter
        tests/test_post_geometry_filter.cpp
    )
    target_link_libraries(test_post_geometry_filter PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_rgb_convert
        tests/test_post_geometry_rgb_convert.cpp
    )
    target_link_libraries(test_post_geometry_rgb_convert PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_stage_3060
        tests/test_post_geometry_stage_3060.cpp
    )
    target_link_libraries(test_post_geometry_stage_3060 PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_stage_3600
        tests/test_post_geometry_stage_3600.cpp
    )
    target_link_libraries(test_post_geometry_stage_3600 PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_stage_2a00
        tests/test_post_geometry_stage_2a00.cpp
    )
    target_link_libraries(test_post_geometry_stage_2a00 PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_stage_2dd0
        tests/test_post_geometry_stage_2dd0.cpp
    )
    target_link_libraries(test_post_geometry_stage_2dd0 PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_stage_3890
        tests/test_post_geometry_stage_3890.cpp
    )
    target_link_libraries(test_post_geometry_stage_3890 PRIVATE dj1000_native_lib)

    add_executable(test_post_geometry_stage_4810
        tests/test_post_geometry_stage_4810.cpp
    )
    target_link_libraries(test_post_geometry_stage_4810 PRIVATE dj1000_native_lib)

    add_executable(test_post_rgb_stage_3b00
        tests/test_post_rgb_stage_3b00.cpp
    )
    target_link_libraries(test_post_rgb_stage_3b00 PRIVATE dj1000_native_lib)

    add_executable(test_post_rgb_stage_40f0
        tests/test_post_rgb_stage_40f0.cpp
    )
    target_link_libraries(test_post_rgb_stage_40f0 PRIVATE dj1000_native_lib)

    add_executable(test_post_rgb_stage_42a0
        tests/test_post_rgb_stage_42a0.cpp
    )
    target_link_libraries(test_post_rgb_stage_42a0 PRIVATE dj1000_native_lib)

    add_executable(test_pregeometry_normalize
        tests/test_pregeometry_normalize.cpp
    )
    target_link_libraries(test_pregeometry_normalize PRIVATE dj1000_native_lib)

    add_executable(test_pregeometry_pipeline
        tests/test_pregeometry_pipeline.cpp
    )
    target_link_libraries(test_pregeometry_pipeline PRIVATE dj1000_native_lib)

    add_executable(test_source_seed_stage
        tests/test_source_seed_stage.cpp
    )
    target_link_libraries(test_source_seed_stage PRIVATE dj1000_native_lib)

    add_executable(test_bright_vertical_gate
        tests/test_bright_vertical_gate.cpp
    )
    target_link_libraries(test_bright_vertical_gate PRIVATE dj1000_native_lib)

    add_executable(test_source_center_stage
        tests/test_source_center_stage.cpp
    )
    target_link_libraries(test_source_center_stage PRIVATE dj1000_native_lib)

    add_executable(test_source_stage
        tests/test_source_stage.cpp
    )
    target_link_libraries(test_source_stage PRIVATE dj1000_native_lib)

    add_executable(test_quantize_stage
        tests/test_quantize_stage.cpp
    )
    target_link_libraries(test_quantize_stage PRIVATE dj1000_native_lib)

    add_executable(test_converter
        tests/test_converter.cpp
    )
    target_link_libraries(test_converter PRIVATE dj1000_native_lib)

    add_executable(test_modern_pipeline
        tests/test_modern_pipeline.cpp
    )
    target_link_libraries(test_modern_pipeline PRIVATE dj1000_native_lib)

    add_executable(test_modern_raw_frame
        tests/test_modern_raw_frame.cpp
    )
    target_link_libraries(test_modern_raw_frame PRIVATE dj1000_native_lib)

    add_executable(test_modern_sensor_frame
        tests/test_modern_sensor_frame.cpp
    )
    target_link_libraries(test_modern_sensor_frame PRIVATE dj1000_native_lib)

    add_executable(test_modern_dng
        tests/test_modern_dng.cpp
    )
    target_link_libraries(test_modern_dng PRIVATE dj1000_native_lib)

    if(DJ1000_HAVE_ADOBE_DNG_SDK)
        add_executable(test_modern_dng_sdk
            tests/test_modern_dng_sdk.cpp
        )
        target_link_libraries(test_modern_dng_sdk PRIVATE dj1000_native_lib)
    endif()

    add_executable(test_session
        tests/test_session.cpp
    )
    target_link_libraries(test_session PRIVATE dj1000_native_lib)

    add_executable(test_c_api
        tests/test_c_api.cpp
    )
    target_link_libraries(test_c_api PRIVATE dj1000_native_lib)

    enable_testing()
    add_test(NAME test_index_view COMMAND test_index_view)
    add_test(NAME test_resample_lut COMMAND test_resample_lut)
    add_test(NAME test_geometry_copy COMMAND test_geometry_copy)
    add_test(NAME test_large_vertical_resample COMMAND test_large_vertical_resample)
    add_test(NAME test_nonlarge_row_resample COMMAND test_nonlarge_row_resample)
    add_test(NAME test_nonlarge_geometry COMMAND test_nonlarge_geometry)
    add_test(NAME test_nonlarge_source_pipeline COMMAND test_nonlarge_source_pipeline)
    add_test(NAME test_post_geometry_prepare COMMAND test_post_geometry_prepare)
    add_test(NAME test_post_geometry_center_scale COMMAND test_post_geometry_center_scale)
    add_test(NAME test_post_geometry_dual_scale COMMAND test_post_geometry_dual_scale)
    add_test(NAME test_post_geometry_edge_response COMMAND test_post_geometry_edge_response)
    add_test(NAME test_post_geometry_filter COMMAND test_post_geometry_filter)
    add_test(NAME test_post_geometry_rgb_convert COMMAND test_post_geometry_rgb_convert)
    add_test(NAME test_post_geometry_stage_3060 COMMAND test_post_geometry_stage_3060)
    add_test(NAME test_post_geometry_stage_3600 COMMAND test_post_geometry_stage_3600)
    add_test(NAME test_post_geometry_stage_2a00 COMMAND test_post_geometry_stage_2a00)
    add_test(NAME test_post_geometry_stage_2dd0 COMMAND test_post_geometry_stage_2dd0)
    add_test(NAME test_post_geometry_stage_3890 COMMAND test_post_geometry_stage_3890)
    add_test(NAME test_post_geometry_stage_4810 COMMAND test_post_geometry_stage_4810)
    add_test(NAME test_post_rgb_stage_3b00 COMMAND test_post_rgb_stage_3b00)
    add_test(NAME test_post_rgb_stage_40f0 COMMAND test_post_rgb_stage_40f0)
    add_test(NAME test_post_rgb_stage_42a0 COMMAND test_post_rgb_stage_42a0)
    add_test(NAME test_pregeometry_normalize COMMAND test_pregeometry_normalize)
    add_test(NAME test_pregeometry_pipeline COMMAND test_pregeometry_pipeline)
    add_test(NAME test_source_seed_stage COMMAND test_source_seed_stage)
    add_test(NAME test_bright_vertical_gate COMMAND test_bright_vertical_gate)
    add_test(NAME test_source_center_stage COMMAND test_source_center_stage)
    add_test(NAME test_source_stage COMMAND test_source_stage)
    add_test(NAME test_quantize_stage COMMAND test_quantize_stage)
    add_test(NAME test_converter COMMAND test_converter)
    add_test(NAME test_modern_pipeline COMMAND test_modern_pipeline)
    add_test(NAME test_modern_raw_frame COMMAND test_modern_raw_frame)
    add_test(NAME test_modern_sensor_frame COMMAND test_modern_sensor_frame)
    add_test(NAME test_modern_dng COMMAND test_modern_dng)
    if(DJ1000_HAVE_ADOBE_DNG_SDK)
        add_test(NAME test_modern_dng_sdk COMMAND test_modern_dng_sdk)
    endif()
    add_test(NAME test_session COMMAND test_session)
    add_test(NAME test_c_api COMMAND test_c_api)
endif()

if(DJ1000_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "DJ1000_BUILD_WASM requires Emscripten. Configure with emcmake cmake ...")
    endif()

    add_executable(dj1000_wasm
        src/wasm_api.cpp
    )
    target_link_libraries(dj1000_wasm PRIVATE dj1000_native_lib)
    target_compile_options(dj1000_wasm PRIVATE -fexceptions)
    if(DJ1000_HAVE_ADOBE_DNG_SDK)
        target_compile_definitions(dj1000_wasm PRIVATE DJ1000_HAVE_ADOBE_DNG_SDK=1)
    else()
        target_compile_definitions(dj1000_wasm PRIVATE DJ1000_HAVE_ADOBE_DNG_SDK=0)
    endif()
    set_target_properties(dj1000_wasm PROPERTIES
        OUTPUT_NAME dj1000_wasm
        SUFFIX .mjs
    )
    target_link_options(dj1000_wasm PRIVATE
        --no-entry
        -fexceptions
        -sALLOW_MEMORY_GROWTH=1
        -sDISABLE_EXCEPTION_CATCHING=0
        -sENVIRONMENT=web,worker,node
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createDj1000Module
        -sEXPORTED_FUNCTIONS=[_malloc,_free,_dj1000_wasm_dng_support_available,_dj1000_wasm_convert_dat_rgba,_dj1000_wasm_convert_dat_dng,_dj1000_wasm_session_open,_dj1000_wasm_session_render_rgba,_dj1000_wasm_session_free,_dj1000_wasm_free_buffer,_dj1000_wasm_free_string]
        -sEXPORTED_RUNTIME_METHODS=[UTF8ToString,HEAP32,HEAPU8,HEAPU32]
        -sFILESYSTEM=0
        -sMODULARIZE=1
        -sNO_EXIT_RUNTIME=1
    )
    if(DJ1000_HAVE_ADOBE_DNG_SDK)
        target_link_options(dj1000_wasm PRIVATE -sUSE_ZLIB=1)
    endif()
    add_custom_command(
        TARGET dj1000_wasm POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/wasm/dj1000_wasm_api.mjs
            $<TARGET_FILE_DIR:dj1000_wasm>/dj1000_wasm_api.mjs
    )
endif()

if(DJ1000_INSTALL)
    if(DJ1000_HAVE_ADOBE_DNG_SDK)
        install(
            TARGETS dj1000_adobe_dng_sdk dj1000_libjpeg
            EXPORT dj1000Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(
        TARGETS dj1000_native_lib
        EXPORT dj1000Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    if(DJ1000_BUILD_CLI)
        install(
            TARGETS dj1000
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PATTERN ".DS_Store" EXCLUDE
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dj1000Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/dj1000Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dj1000
    )
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/dj1000ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        EXPORT dj1000Targets
        FILE dj1000Targets.cmake
        NAMESPACE dj1000::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dj1000
    )
    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/dj1000Config.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/dj1000ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dj1000
    )
endif()
