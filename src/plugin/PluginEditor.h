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
    juce::Label title, status, strengthLabel, targetLabel, mixLabel, waveformLabel;
    juce::Label hits, peak, certainty, gain;
    juce::Slider strength, targetLevel, mix;
    juce::ComboBox targetMode;
    WaveformScope scope;
    std::vector<float> scopeScratch = std::vector<float>(2048, 0.0f);
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment, targetAttachment, mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetModeAttachment;
};
