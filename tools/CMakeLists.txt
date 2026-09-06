add_library(beat_leveler_harness STATIC Harness.cpp)
target_include_directories(beat_leveler_harness PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(beat_leveler_harness PUBLIC beat_leveler_dsp
    PRIVATE beat_leveler_warnings)
target_compile_definitions(beat_leveler_harness PRIVATE
    BEAT_LEVELER_VERSION="${PROJECT_VERSION}")

if(BEAT_LEVELER_BUILD_TOOLS)
    add_library(beat_leveler_wav STATIC WavFiles.cpp)
    target_link_libraries(beat_leveler_wav PUBLIC beat_leveler_harness
        PRIVATE juce::juce_audio_formats beat_leveler_warnings)
    target_compile_definitions(beat_leveler_wav PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_USE_MP3AUDIOFORMAT=0)

    add_executable(beat_leveler_runner main.cpp)
    target_link_libraries(beat_leveler_runner PRIVATE beat_leveler_wav beat_leveler_warnings)
endif()
