#pragma once

#include "PluginProcessor.h"
#include "ScopeView.h"

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
    juce::Label title, status, strengthLabel, levelLabel, targetLabel, modeLabel, holdLabel, releaseLabel, curveLabel, mixLabel, scopeLabel;
    juce::Label hits, peak, spread, gain;
    juce::Slider strength, targetLevel, autoLevel, hold, release, mix;
    juce::ComboBox targetMode, releaseCurve, levelingMode;
    juce::TooltipWindow tooltips { this };
    ScopeView scope;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> curveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetModeAttachment;
};
