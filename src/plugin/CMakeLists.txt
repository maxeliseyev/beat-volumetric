# Development identifiers only: approve final manufacturer/plugin/bundle IDs before
# distributing any build. They are intentionally not presented as release metadata.
juce_add_plugin(BeatVolumetric
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Beat Volumetric Development"
    BUNDLE_ID com.beatvolumetric.development
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Bvlt
    PLUGIN_CODE Lvld
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "BeatVolumetricDev"
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(BeatVolumetric PRIVATE PluginProcessor.cpp PluginEditor.cpp WaveformScope.cpp)
target_link_libraries(BeatVolumetric
    PRIVATE beat_leveler_dsp juce::juce_audio_utils juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
target_compile_definitions(BeatVolumetric PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_REPORT_APP_USAGE=0)
