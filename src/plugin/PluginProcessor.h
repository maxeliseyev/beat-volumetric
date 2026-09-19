#pragma once

#include "dsp/StreamingLeveler.h"
#include "ScopeRing.h"

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

class BeatVolumetricAudioProcessor final : public juce::AudioProcessor
{
public:
    BeatVolumetricAudioProcessor();

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return parameters; }
    std::uint64_t detectedHits() const noexcept { return hitCount.load(); }
    float lastPeakDbfs() const noexcept { return lastPeak.load(); }
    float lastGainDb() const noexcept { return gain.load(); }
    float lastConfidence() const noexcept { return confidence.load(); }
    void copyScope(float* destination, std::size_t count) const noexcept { scope.copyLast(destination, count); }
    std::int64_t scopeSample() const noexcept { return scope.currentSample(); }
    std::size_t scopeLength() const noexcept { return scope.length(); }
    std::int64_t lastOnsetSample() const noexcept { return onsetSample.load(); }

private:
    static BusesProperties buses();
    static juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout();

    juce::AudioProcessorValueTreeState parameters;
    beat::leveler::StreamingLeveler leveler;
    std::array<beat::leveler::OnsetEvent, 32> events {};
    std::array<beat::leveler::HitMeasurement, 32> measurements {};
    std::atomic<std::uint64_t> hitCount { 0 };
    std::atomic<float> lastPeak { -240.0f };
    std::atomic<float> gain { 0.0f };
    std::atomic<float> confidence { 0.0f };
    std::atomic<std::int64_t> onsetSample { -1 };
    ScopeRing scope;
    std::atomic<float>* strengthParameter = nullptr;
    std::atomic<float>* targetModeParameter = nullptr;
    std::atomic<float>* targetDbfsParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatVolumetricAudioProcessor)
};
