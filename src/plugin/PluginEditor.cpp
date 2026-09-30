#include "PluginEditor.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace
{

float deviation(const float* values, std::size_t count)
{
    if (count < 2)
        return 0.0f;
    double sum = 0.0;
    for (std::size_t index = 0; index < count; ++index)
        sum += values[index];
    const auto mean = sum / static_cast<double>(count);
    double energy = 0.0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto delta = static_cast<double>(values[index]) - mean;
        energy += delta * delta;
    }
    return static_cast<float>(std::sqrt(energy / static_cast<double>(count)));
}

} // namespace

BeatVolumetricAudioProcessorEditor::BeatVolumetricAudioProcessorEditor(BeatVolumetricAudioProcessor& value)
    : AudioProcessorEditor(&value), volumetricProcessor(value)
{
    title.setText("Beat Volumetric", juce::dontSendNotification);
    status.setText("Realtime leveler  ·  56 ms lookahead", juce::dontSendNotification);
    title.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
    for (auto* label : { &strengthLabel, &targetLabel, &levelLabel, &windowLabel, &mixLabel, &waveformLabel })
    {
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
        label->setJustificationType(juce::Justification::centred);
    }
    strengthLabel.setText("SMOOTHING", juce::dontSendNotification);
    targetLabel.setText("TARGET", juce::dontSendNotification);
    targetLabel.setJustificationType(juce::Justification::centredLeft);
    levelLabel.setText("LEVEL", juce::dontSendNotification);
    windowLabel.setText("WINDOW", juce::dontSendNotification);
    mixLabel.setText("DRY / WET", juce::dontSendNotification);
    waveformLabel.setText("INPUT  ·  HIT LEVEL BEFORE AND AFTER", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centredLeft);
    for (auto* label : { &hits, &peak, &spread, &gain })
    {
        label->setFont(juce::FontOptions(14.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xff6dd3b5));
    }
    targetMode.addItem("Auto", 1);
    targetMode.addItem("Manual", 2);
    for (auto* slider : { &strength, &targetLevel, &window, &mix })
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 20);
        addAndMakeVisible(*slider);
    }
    strength.setNumDecimalPlacesToDisplay(0);
    targetLevel.setNumDecimalPlacesToDisplay(1);
    targetLevel.setTextValueSuffix(" dB");
    window.setNumDecimalPlacesToDisplay(0);
    mix.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(targetMode);
    addAndMakeVisible(scope);
    for (auto* label : { &title, &status, &strengthLabel, &targetLabel, &levelLabel, &windowLabel,
                         &mixLabel, &waveformLabel, &hits, &peak, &spread, &gain })
        addAndMakeVisible(*label);

    auto& state = volumetricProcessor.state();
    strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "strength", strength);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "target_dbfs", targetLevel);
    windowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "window_ms", window);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "mix", mix);
    targetModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "target_mode", targetMode);
    targetMode.onChange = [this] { showManualTarget(targetMode.getSelectedId() == 2); };
    showManualTarget(targetMode.getSelectedId() == 2);
    scopeHits.reserve(16);
    setSize(740, 700);
    startTimerHz(20);
}

void BeatVolumetricAudioProcessorEditor::showManualTarget(bool manual)
{
    levelLabel.setVisible(manual);
    targetLevel.setVisible(manual);
    resized();
}

void BeatVolumetricAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff15171c));
    graphics.setColour(juce::Colour(0xff6dd3b5));
    graphics.fillRoundedRectangle(getLocalBounds().reduced(12).toFloat(), 12.0f);
    graphics.setColour(juce::Colour(0xff15171c));
    graphics.fillRoundedRectangle(getLocalBounds().reduced(14).toFloat(), 10.0f);
}

void BeatVolumetricAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    title.setBounds(area.removeFromTop(32));
    status.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    auto controls = area.removeFromTop(280);
    auto hero = controls.removeFromLeft(230);
    strengthLabel.setBounds(hero.removeFromTop(22));
    strength.setBounds(hero.reduced(4, 0));

    controls.removeFromLeft(28);
    auto mode = controls.removeFromTop(30);
    targetLabel.setBounds(mode.removeFromLeft(78));
    targetMode.setBounds(mode.removeFromLeft(150).reduced(0, 1));

    controls.removeFromTop(18);
    auto knobs = controls.removeFromTop(168);
    const auto manual = targetLevel.isVisible();
    const auto columnWidth = manual ? 128 : 170;
    auto place = [&](juce::Label& label, juce::Slider& slider)
    {
        auto column = knobs.removeFromLeft(columnWidth);
        knobs.removeFromLeft(12);
        label.setBounds(column.removeFromTop(18));
        slider.setBounds(column);
    };
    if (manual)
        place(levelLabel, targetLevel);
    place(windowLabel, window);
    place(mixLabel, mix);

    auto telemetry = area.removeFromBottom(36);
    hits.setBounds(telemetry.removeFromLeft(120));
    peak.setBounds(telemetry.removeFromLeft(160));
    gain.setBounds(telemetry.removeFromRight(130));
    spread.setBounds(telemetry);

    waveformLabel.setBounds(area.removeFromTop(22));
    scope.setBounds(area.reduced(0, 2));
}

void BeatVolumetricAudioProcessorEditor::timerCallback()
{
    const auto length = volumetricProcessor.scopeLength();
    if (scopeScratch.size() != length)
        scopeScratch.assign(length, 0.0f);
    if (!scopeScratch.empty())
        volumetricProcessor.copyScope(scopeScratch.data(), scopeScratch.size());
    scope.setWaveform(scopeScratch.data(), scopeScratch.size());

    std::array<HitTelemetrySample, 16> marks {};
    const auto markCount = volumetricProcessor.copyHits(marks.data(), marks.size());
    const auto end = volumetricProcessor.scopeSample();
    const auto windowSamples = static_cast<std::int64_t>(length);
    std::array<float, 16> before {};
    std::array<float, 16> after {};
    scopeHits.clear();
    for (std::size_t index = 0; index < markCount; ++index)
    {
        const auto& mark = marks[index];
        before[index] = mark.measuredDb;
        after[index] = mark.measuredDb + mark.gainDb;
        if (windowSamples <= 0)
            continue;
        const auto position = static_cast<float>(mark.onsetSample - (end - windowSamples))
                              / static_cast<float>(windowSamples);
        if (position >= 0.0f && position <= 1.0f)
            scopeHits.push_back({ position, mark.measuredDb, mark.measuredDb + mark.gainDb });
    }
    scope.setHits(scopeHits.data(), scopeHits.size());

    hits.setText("Hits  " + juce::String(static_cast<juce::int64>(volumetricProcessor.detectedHits())),
                 juce::dontSendNotification);
    peak.setText("Peak  " + juce::String(volumetricProcessor.lastPeakDbfs(), 1) + " dBFS",
                 juce::dontSendNotification);
    if (markCount >= 2)
    {
        spread.setText("Spread  " + juce::String(deviation(before.data(), markCount), 1)
                           + " → " + juce::String(deviation(after.data(), markCount), 1) + " dB",
                       juce::dontSendNotification);
    }
    else
    {
        spread.setText("Spread  —", juce::dontSendNotification);
    }
    gain.setText("Gain  " + juce::String(volumetricProcessor.lastGainDb(), 1) + " dB",
                 juce::dontSendNotification);
}
