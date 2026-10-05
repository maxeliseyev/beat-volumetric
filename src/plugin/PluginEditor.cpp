#include "PluginEditor.h"

#include <algorithm>
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
    title.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
    for (auto* label : { &strengthLabel, &levelLabel, &targetLabel, &windowLabel, &mixLabel, &historyLabel })
    {
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
        label->setJustificationType(juce::Justification::centred);
    }
    strengthLabel.setText("STRENGTH", juce::dontSendNotification);
    levelLabel.setText("LEVEL", juce::dontSendNotification);
    targetLabel.setText("TARGET", juce::dontSendNotification);
    targetLabel.setJustificationType(juce::Justification::centredLeft);
    windowLabel.setText("WINDOW", juce::dontSendNotification);
    windowLabel.setJustificationType(juce::Justification::centredLeft);
    mixLabel.setText("DRY / WET", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centredLeft);
    historyLabel.setText("GAIN CHANGE  ·  UP IS BOOST, DOWN IS CUT, BARS STAY", juce::dontSendNotification);
    historyLabel.setJustificationType(juce::Justification::centredLeft);
    for (auto* label : { &hits, &peak, &spread, &gain })
    {
        label->setFont(juce::FontOptions(14.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xff6dd3b5));
    }

    targetMode.addItem("Auto", 1);
    targetMode.addItem("Manual", 2);
    for (auto* slider : { &strength, &targetLevel, &autoLevel })
    {
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
        addAndMakeVisible(*slider);
    }
    for (auto* slider : { &window, &mix })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 20);
        addAndMakeVisible(*slider);
    }
    autoLevel.setRange(-80.0, 0.0, 0.1);
    autoLevel.setTextValueSuffix(" dB");
    autoLevel.setNumDecimalPlacesToDisplay(1);
    autoLevel.setValue(-12.0, juce::dontSendNotification);
    // A disabled slider greys out and reads as bypass. Keep the paint live and ignore the mouse.
    autoLevel.setTextBoxIsEditable(false);
    autoLevel.setInterceptsMouseClicks(false, false);
    for (auto* slider : { &strength, &targetLevel, &autoLevel })
    {
        slider->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff222831));
        slider->setColour(juce::Slider::trackColourId, juce::Colour(0xff6dd3b5));
        slider->setColour(juce::Slider::thumbColourId, juce::Colour(0xfff4f7fb));
    }
    strength.setNumDecimalPlacesToDisplay(0);
    targetLevel.setNumDecimalPlacesToDisplay(1);
    window.setNumDecimalPlacesToDisplay(0);
    mix.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(targetMode);
    addAndMakeVisible(history);
    for (auto* label : { &title, &status, &strengthLabel, &levelLabel, &targetLabel, &windowLabel,
                         &mixLabel, &historyLabel, &hits, &peak, &spread, &gain })
        addAndMakeVisible(*label);

    auto& state = volumetricProcessor.state();
    strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "strength", strength);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "target_dbfs", targetLevel);
    windowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "window_ms", window);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "mix", mix);
    targetModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "target_mode", targetMode);
    targetMode.onChange = [this] { showManualTarget(targetMode.getSelectedId() == 2); };
    showManualTarget(targetMode.getSelectedId() == 2);
    historyBars.reserve(16);
    setSize(760, 680);
    startTimerHz(20);
}

void BeatVolumetricAudioProcessorEditor::showManualTarget(bool manual)
{
    levelLabel.setText(manual ? "LEVEL" : "AUTO", juce::dontSendNotification);
    targetLevel.setVisible(manual);
    autoLevel.setVisible(!manual);
    status.setText(manual
                       ? "Level sets how loud every hit should be."
                       : "Auto aims at the middle of recent hits.",
                   juce::dontSendNotification);
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
    status.setBounds(area.removeFromTop(36));
    area.removeFromTop(6);

    auto controls = area.removeFromTop(250);
    auto strengthColumn = controls.removeFromLeft(96);
    strengthLabel.setBounds(strengthColumn.removeFromTop(18));
    strength.setBounds(strengthColumn.reduced(4, 0));
    controls.removeFromLeft(8);
    auto levelColumn = controls.removeFromLeft(96);
    levelLabel.setBounds(levelColumn.removeFromTop(18));
    targetLevel.setBounds(levelColumn.reduced(4, 0));
    autoLevel.setBounds(levelColumn.reduced(4, 0));

    controls.removeFromLeft(24);
    auto mode = controls.removeFromTop(28);
    targetLabel.setBounds(mode.removeFromLeft(72));
    targetMode.setBounds(mode.removeFromLeft(160).reduced(0, 1));
    controls.removeFromTop(28);

    auto placeSlider = [&](juce::Label& label, juce::Slider& slider)
    {
        auto row = controls.removeFromTop(28);
        label.setBounds(row.removeFromLeft(84));
        slider.setBounds(row);
        controls.removeFromTop(10);
    };
    placeSlider(windowLabel, window);
    placeSlider(mixLabel, mix);

    auto telemetry = area.removeFromBottom(36);
    hits.setBounds(telemetry.removeFromLeft(120));
    peak.setBounds(telemetry.removeFromLeft(160));
    gain.setBounds(telemetry.removeFromRight(130));
    spread.setBounds(telemetry);

    historyLabel.setBounds(area.removeFromTop(22));
    history.setBounds(area.reduced(0, 2));
}

void BeatVolumetricAudioProcessorEditor::timerCallback()
{
    std::array<HitTelemetrySample, 16> marks {};
    const auto markCount = volumetricProcessor.copyHits(marks.data(), marks.size());
    std::sort(marks.begin(), marks.begin() + static_cast<std::ptrdiff_t>(markCount),
              [](const HitTelemetrySample& left, const HitTelemetrySample& right)
              {
                  return left.onsetSample < right.onsetSample;
              });

    std::array<float, 16> before {};
    std::array<float, 16> after {};
    historyBars.clear();
    for (std::size_t index = 0; index < markCount; ++index)
    {
        before[index] = marks[index].measuredDb;
        after[index] = marks[index].measuredDb + marks[index].gainDb;
        historyBars.push_back({ marks[index].gainDb });
    }
    history.setBars(historyBars.data(), historyBars.size());

    if (!targetLevel.isVisible())
    {
        if (markCount > 0)
        {
            const auto target = marks[markCount - 1].targetDb;
            autoLevel.setValue(target, juce::dontSendNotification);
            status.setText("Auto aims at " + juce::String(target, 1)
                               + " dB, the middle of recent hits.",
                           juce::dontSendNotification);
        }
        else
        {
            status.setText("Auto aims at the middle of recent hits.",
                           juce::dontSendNotification);
        }
    }

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
