#include "PluginEditor.h"

BeatVolumetricAudioProcessorEditor::BeatVolumetricAudioProcessorEditor(BeatVolumetricAudioProcessor& value)
    : AudioProcessorEditor(&value), volumetricProcessor(value)
{
    title.setText("Beat Volumetric", juce::dontSendNotification);
    status.setText("Realtime leveler active - 56 ms lookahead", juce::dontSendNotification);
    title.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
    for (auto* label : { &strengthLabel, &targetLabel, &mixLabel, &waveformLabel })
    {
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
    }
    strengthLabel.setText("STRENGTH", juce::dontSendNotification);
    targetLabel.setText("TARGET LEVEL", juce::dontSendNotification);
    mixLabel.setText("DRY / WET", juce::dontSendNotification);
    waveformLabel.setText("INPUT WAVEFORM", juce::dontSendNotification);
    for (auto* label : { &hits, &peak, &certainty, &gain })
    {
        label->setFont(juce::FontOptions(14.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xff6dd3b5));
    }
    targetMode.addItem("Auto", 1); targetMode.addItem("Manual", 2);
    for (auto* slider : { &strength, &targetLevel, &mix })
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 20);
        addAndMakeVisible(*slider);
    }
    strength.setNumDecimalPlacesToDisplay(2);
    targetLevel.setNumDecimalPlacesToDisplay(1);
    mix.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(targetMode);
    addAndMakeVisible(scope);
    for (auto* label : { &title, &status, &strengthLabel, &targetLabel, &mixLabel,
                         &waveformLabel, &hits, &peak, &certainty, &gain })
        addAndMakeVisible(*label);
    auto& state = volumetricProcessor.state();
    strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "strength", strength);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "target_dbfs", targetLevel);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "mix", mix);
    targetModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "target_mode", targetMode);
    setSize(620, 520);
    startTimerHz(20);
}

void BeatVolumetricAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15171c));
    g.setColour(juce::Colour(0xff6dd3b5)); g.fillRoundedRectangle(getLocalBounds().reduced(12).toFloat(), 12.0f);
    g.setColour(juce::Colour(0xff15171c)); g.fillRoundedRectangle(getLocalBounds().reduced(14).toFloat(), 10.0f);
}

void BeatVolumetricAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    title.setBounds(area.removeFromTop(32));
    status.setBounds(area.removeFromTop(28));

    auto controls = area.removeFromTop(238).reduced(22, 10);
    targetMode.setBounds(controls.removeFromTop(30).removeFromLeft(160));
    controls.removeFromTop(12);
    auto knobs = controls.removeFromTop(188);

    auto strengthArea = knobs.removeFromLeft(160);
    strengthLabel.setBounds(strengthArea.removeFromTop(22));
    strength.setBounds(strengthArea);
    knobs.removeFromLeft(18);

    auto targetArea = knobs.removeFromLeft(160);
    targetLabel.setBounds(targetArea.removeFromTop(22));
    targetLevel.setBounds(targetArea);
    knobs.removeFromLeft(18);

    auto mixArea = knobs.removeFromLeft(160);
    mixLabel.setBounds(mixArea.removeFromTop(22));
    mix.setBounds(mixArea);

    auto telemetry = area.removeFromBottom(50);
    hits.setBounds(telemetry.removeFromLeft(140));
    peak.setBounds(telemetry.removeFromLeft(150));
    certainty.setBounds(telemetry.removeFromLeft(150));
    gain.setBounds(telemetry);

    waveformLabel.setBounds(area.removeFromTop(22));
    scope.setBounds(area.reduced(0, 2));
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
    gain.setText("Gain  " + juce::String(volumetricProcessor.lastGainDb(), 1) + " dB", juce::dontSendNotification);
}
