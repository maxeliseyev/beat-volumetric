#include "PluginEditor.h"

BeatVolumetricAudioProcessorEditor::BeatVolumetricAudioProcessorEditor(BeatVolumetricAudioProcessor& value)
    : AudioProcessorEditor(&value), volumetricProcessor(value)
{
    title.setText("Beat Volumetric", juce::dontSendNotification);
    status.setText("Detector + meters active — gain scheduling is next", juce::dontSendNotification);
    title.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
    for (auto* label : { &hits, &peak, &certainty })
    {
        label->setFont(juce::FontOptions(16.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xff6dd3b5));
    }
    targetMode.addItem("Auto", 1); targetMode.addItem("Manual", 2);
    for (auto* slider : { &strength, &targetLevel, &mix })
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 20);
        addAndMakeVisible(*slider);
    }
    addAndMakeVisible(targetMode);
    addAndMakeVisible(scope);
    for (auto* label : { &title, &status, &hits, &peak, &certainty }) addAndMakeVisible(*label);
    auto& state = volumetricProcessor.state();
    strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "strength", strength);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "target_dbfs", targetLevel);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "mix", mix);
    targetModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "target_mode", targetMode);
    setSize(560, 460); startTimerHz(20);
}

void BeatVolumetricAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15171c));
    g.setColour(juce::Colour(0xff6dd3b5)); g.fillRoundedRectangle(getLocalBounds().reduced(12).toFloat(), 12.0f);
    g.setColour(juce::Colour(0xff15171c)); g.fillRoundedRectangle(getLocalBounds().reduced(14).toFloat(), 10.0f);
    g.setColour(juce::Colour(0xffa7b0bd));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("STRENGTH", 54, 130, 120, 20, juce::Justification::centred);
    g.drawText("TARGET LEVEL", 194, 130, 120, 20, juce::Justification::centred);
    g.drawText("DRY / WET", 334, 130, 120, 20, juce::Justification::centred);
    g.drawText("INPUT WAVEFORM", 42, 305, 180, 20, juce::Justification::centredLeft);
}

void BeatVolumetricAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    title.setBounds(area.removeFromTop(32)); status.setBounds(area.removeFromTop(28));
    auto telemetry = area.removeFromBottom(72); hits.setBounds(telemetry.removeFromLeft(160));
    peak.setBounds(telemetry.removeFromLeft(160)); certainty.setBounds(telemetry);
    auto controls = area.reduced(22, 28);
    targetMode.setBounds(controls.removeFromTop(28).removeFromLeft(140));
    controls.removeFromTop(16);
    strength.setBounds(controls.removeFromLeft(140));
    targetLevel.setBounds(controls.removeFromLeft(140)); mix.setBounds(controls.removeFromLeft(140));
    scope.setBounds(42, 326, getWidth() - 84, 86);
}

void BeatVolumetricAudioProcessorEditor::timerCallback()
{
    volumetricProcessor.copyScope(scopeScratch.data(), scopeScratch.size());
    scope.setWaveform(scopeScratch.data(), scopeScratch.size());
    const auto end = volumetricProcessor.scopeSample();
    const auto onset = volumetricProcessor.lastOnsetSample();
    const auto length = static_cast<std::int64_t>(volumetricProcessor.scopeLength());
    const auto position = length > 0 ? static_cast<float>(onset - (end - length)) / static_cast<float>(length) : -1.0f;
    scope.setMarker(position, volumetricProcessor.lastConfidence());
    hits.setText("Hits  " + juce::String(static_cast<juce::int64>(volumetricProcessor.detectedHits())), juce::dontSendNotification);
    peak.setText("Peak  " + juce::String(volumetricProcessor.lastPeakDbfs(), 1) + " dBFS", juce::dontSendNotification);
    certainty.setText("Confidence  " + juce::String(100.0f * volumetricProcessor.lastConfidence(), 0) + "%", juce::dontSendNotification);
}
