add_executable(beat_leveler_tests test_harness.cpp test_streaming_contract.cpp)
target_link_libraries(beat_leveler_tests PRIVATE beat_leveler_harness
    Catch2::Catch2WithMain beat_leveler_warnings)
add_test(NAME beat_leveler_tests COMMAND beat_leveler_tests)

if(BEAT_LEVELER_BUILD_TOOLS)
    add_test(NAME beat_leveler_cli
        COMMAND "${CMAKE_COMMAND}"
            "-DRUNNER=$<TARGET_FILE:beat_leveler_runner>"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.cmake")
endif()
