get_filename_component(_TEST_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

add_executable(${_TEST_NAME}_tests
    test_corkscrew_partition.cpp
    test_dlp_config.cpp
    test_dlp_debug_log.cpp
)

target_link_libraries(${_TEST_NAME}_tests test_common TBB::tbb TBB::tbbmalloc libslic3r)
set_property(TARGET ${_TEST_NAME}_tests PROPERTY FOLDER "tests")

if (WIN32)
    prusaslicer_copy_dlls(${_TEST_NAME}_tests)
endif()

add_test(${_TEST_NAME}_tests ${_TEST_NAME}_tests ${CATCH_EXTRA_ARGS})
