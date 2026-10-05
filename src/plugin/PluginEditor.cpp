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
    for (auto* label : { &strengthLabel, &levelLabel, &targetLabel, &modeLabel, &sourceLabel, &holdLabel, &releaseLabel, &curveLabel, &mixLabel, &scopeLabel })
    {
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa7b0bd));
        label->setJustificationType(juce::Justification::centred);
    }
    strengthLabel.setText("STRENGTH", juce::dontSendNotification);
    levelLabel.setText("LEVEL", juce::dontSendNotification);
    targetLabel.setText("TARGET", juce::dontSendNotification);
    targetLabel.setJustificationType(juce::Justification::centredLeft);
    modeLabel.setText("MODE", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centredLeft);
    levelingMode.setTooltip("Both levels in both directions. Cut loud only turns loud hits down to the level. Lift quiet only turns quiet hits up.");
    levelingMode.addItem("Both", 1);
    levelingMode.addItem("Cut loud", 2);
    levelingMode.addItem("Lift quiet", 3);
    addAndMakeVisible(levelingMode);
    sourceLabel.setText("DETECT", juce::dontSendNotification);
    sourceLabel.setJustificationType(juce::Justification::centredLeft);
    detectSource.setTooltip("What hits are detected and measured on. Stereo uses both channels. Mid uses L+R, favouring centred hits over wide cymbals, but an out-of-phase hit cancels. Peak uses the louder channel. The gain is always the same on both channels.");
    detectSource.addItem("Stereo", 1);
    detectSource.addItem("Mid", 2);
    detectSource.addItem("Peak", 3);
    addAndMakeVisible(detectSource);
    holdLabel.setText("HOLD", juce::dontSendNotification);
    releaseLabel.setText("RELEASE", juce::dontSendNotification);
    curveLabel.setText("CURVE", juce::dontSendNotification);
    for (auto* label : { &holdLabel, &releaseLabel, &curveLabel })
        label->setJustificationType(juce::Justification::centredLeft);
    hold.setTooltip("How long each hit keeps its corrected level after the onset, in ms.");
    release.setTooltip("How long the level takes to glide back to the original after Hold, in ms.");
    releaseCurve.setTooltip("Linear returns at a steady rate. Curved eases out of Hold and into the original level.");
    mixLabel.setText("DRY / WET", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centredLeft);
    scopeLabel.setText("LAST 2 BARS  ·  YELLOW IS WHAT THE PLUGIN CHANGED", juce::dontSendNotification);
    scopeLabel.setJustificationType(juce::Justification::centredLeft);
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
    for (auto* slider : { &hold, &release, &mix })
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
    hold.setNumDecimalPlacesToDisplay(0);
    release.setNumDecimalPlacesToDisplay(0);
    releaseCurve.addItem("Linear", 1);
    releaseCurve.addItem("Curved", 2);
    addAndMakeVisible(releaseCurve);
    mix.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(targetMode);
    addAndMakeVisible(scope);
    for (auto* label : { &title, &status, &strengthLabel, &levelLabel, &targetLabel, &modeLabel, &sourceLabel, &holdLabel, &releaseLabel, &curveLabel,
                         &mixLabel, &scopeLabel, &hits, &peak, &spread, &gain })
        addAndMakeVisible(*label);

    auto& state = volumetricProcessor.state();
    strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "strength", strength);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "target_dbfs", targetLevel);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "hold_ms", hold);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "release_ms", release);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "mode", levelingMode);
    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "detect_source", detectSource);
    curveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "release_curve", releaseCurve);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "mix", mix);
    targetModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "target_mode", targetMode);
    targetMode.onChange = [this] { showManualTarget(targetMode.getSelectedId() == 2); };
    showManualTarget(targetMode.getSelectedId() == 2);
    setSize(960, 720);
    startTimerHz(30);
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
    status.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);

    auto telemetry = area.removeFromBottom(32);
    hits.setBounds(telemetry.removeFromLeft(120));
    peak.setBounds(telemetry.removeFromLeft(170));
    gain.setBounds(telemetry.removeFromRight(140));
    spread.setBounds(telemetry);
    area.removeFromBottom(8);

    auto controls = area.removeFromLeft(220);
    area.removeFromLeft(16);
    scopeLabel.setBounds(area.removeFromTop(20));
    scope.setBounds(area);

    auto faders = controls.removeFromTop(std::max(180, controls.getHeight() - 262));
    auto strengthColumn = faders.removeFromLeft(104);
    strengthLabel.setBounds(strengthColumn.removeFromTop(18));
    strength.setBounds(strengthColumn.reduced(6, 0));
    auto levelColumn = faders;
    levelLabel.setBounds(levelColumn.removeFromTop(18));
    targetLevel.setBounds(levelColumn.reduced(6, 0));
    autoLevel.setBounds(levelColumn.reduced(6, 0));

    controls.removeFromTop(8);
    auto mode = controls.removeFromTop(28);
    targetLabel.setBounds(mode.removeFromLeft(64));
    targetMode.setBounds(mode.reduced(0, 1));
    controls.removeFromTop(8);

    auto placeSlider = [&](juce::Label& label, juce::Slider& slider)
    {
        auto row = controls.removeFromTop(28);
        label.setBounds(row.removeFromLeft(72));
        slider.setBounds(row);
        controls.removeFromTop(6);
    };
    auto modeRow = controls.removeFromTop(28);
    modeLabel.setBounds(modeRow.removeFromLeft(72));
    levelingMode.setBounds(modeRow.reduced(0, 1));
    controls.removeFromTop(6);
    auto sourceRow = controls.removeFromTop(28);
    sourceLabel.setBounds(sourceRow.removeFromLeft(72));
    detectSource.setBounds(sourceRow.reduced(0, 1));
    controls.removeFromTop(6);
    placeSlider(holdLabel, hold);
    placeSlider(releaseLabel, release);
    auto curveRow = controls.removeFromTop(28);
    curveLabel.setBounds(curveRow.removeFromLeft(72));
    releaseCurve.setBounds(curveRow.reduced(0, 1));
    controls.removeFromTop(6);
    placeSlider(mixLabel, mix);
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
    for (std::size_t index = 0; index < markCount; ++index)
    {
        before[index] = marks[index].measuredDb;
        after[index] = marks[index].measuredDb + marks[index].gainDb;
    }
    HitScope::Frame frame;
    if (volumetricProcessor.copyScope(frame))
        scope.setFrame(frame);

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
