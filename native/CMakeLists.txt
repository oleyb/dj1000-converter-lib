cmake_minimum_required(VERSION 3.20)

project(dj1000 VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DJ1000_BUILD_CLI "Build the standalone dj1000 CLI" ON)
option(DJ1000_BUILD_TESTS "Build the native test suite" ON)
option(DJ1000_INSTALL "Enable install/export packaging targets" ON)

add_library(dj1000_native_lib
    src/bmp.cpp
    src/bright_vertical_gate.cpp
    src/converter.cpp
    src/dat_file.cpp
    src/geometry_copy.cpp
    src/index_view.cpp
    src/large_export_pipeline.cpp
    src/large_vertical_resample.cpp
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
endif()

if(DJ1000_INSTALL)
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
