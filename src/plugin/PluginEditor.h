#pragma once

#include "GainHistory.h"
#include "PluginProcessor.h"

#include <vector>

class BeatVolumetricAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer
{
public:
    explicit BeatVolumetricAudioProcessorEditor(BeatVolumetricAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showManualTarget(bool manual);

    BeatVolumetricAudioProcessor& volumetricProcessor;
    juce::Label title, status, strengthLabel, levelLabel, targetLabel, windowLabel, mixLabel, historyLabel;
    juce::Label hits, peak, spread, gain;
    juce::Slider strength, targetLevel, autoLevel, window, mix;
    juce::ComboBox targetMode;
    GainHistory history;
    std::vector<GainHistory::Bar> historyBars;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> windowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetModeAttachment;
};
