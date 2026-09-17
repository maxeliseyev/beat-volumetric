#pragma once

#include "PluginProcessor.h"
#include "WaveformScope.h"

class BeatVolumetricAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer
{
public:
    explicit BeatVolumetricAudioProcessorEditor(BeatVolumetricAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    BeatVolumetricAudioProcessor& volumetricProcessor;
    juce::Label title, status, hits, peak, certainty;
    juce::Slider strength, targetLevel, mix;
    juce::ComboBox targetMode;
    WaveformScope scope;
    std::vector<float> scopeScratch { 2048 };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment, targetAttachment, mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetModeAttachment;
};
