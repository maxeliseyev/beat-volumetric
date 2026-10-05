#include "GainHistory.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

void GainHistory::setBars(const Bar* marks, std::size_t count)
{
    if (marks == nullptr || count == 0)
        bars.clear();
    else
        bars.assign(marks, marks + count);
    repaint();
}

void GainHistory::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds();
    graphics.setColour(juce::Colour(0xff090b0f));
    graphics.fillRoundedRectangle(bounds.toFloat(), 7.0f);
    graphics.setColour(juce::Colour(0xff364452));
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 7.0f, 1.0f);

    auto plot = bounds.reduced(8, 8);
    auto captions = plot.removeFromBottom(16);
    const auto scaleWidth = 36;
    auto scale = plot.removeFromLeft(scaleWidth);
    const auto top = static_cast<float>(plot.getY());
    const auto bottom = static_cast<float>(plot.getBottom());
    const auto mid = (top + bottom) * 0.5f;
    const auto half = (bottom - top) * 0.5f;
    constexpr float fullScaleDb = 12.0f;

    auto yForGain = [&](float gainDb)
    {
        const auto clamped = std::clamp(gainDb, -fullScaleDb, fullScaleDb);
        return mid - (clamped / fullScaleDb) * half;
    };

    graphics.setFont(juce::FontOptions(11.0f));
    graphics.setColour(juce::Colour(0xff5c6770));
    for (const auto mark : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f })
    {
        const auto y = yForGain(mark);
        graphics.drawHorizontalLine(juce::roundToInt(y),
                                    static_cast<float>(plot.getX()),
                                    static_cast<float>(plot.getRight()));
        graphics.setColour(juce::Colour(0xff8b95a1));
        graphics.drawText(juce::String(juce::roundToInt(mark)),
                          scale.withY(juce::roundToInt(y) - 7).withHeight(14),
                          juce::Justification::centredRight);
        graphics.setColour(juce::Colour(0xff5c6770));
    }

    graphics.setColour(juce::Colour(0xffa7b0bd));
    graphics.drawHorizontalLine(juce::roundToInt(mid),
                                static_cast<float>(plot.getX()),
                                static_cast<float>(plot.getRight()));

    if (bars.empty())
    {
        graphics.setColour(juce::Colour(0xff8b95a1));
        graphics.drawText("Each hit stays here as its gain change",
                          plot, juce::Justification::centred);
        return;
    }

    constexpr int slots = 12;
    const auto shown = std::min(slots, static_cast<int>(bars.size()));
    const auto first = static_cast<int>(bars.size()) - shown;
    const auto slotWidth = static_cast<float>(plot.getWidth()) / static_cast<float>(slots);
    for (int index = 0; index < shown; ++index)
    {
        const auto& bar = bars[static_cast<std::size_t>(first + index)];
        const auto slot = slots - shown + index;
        const auto x = static_cast<float>(plot.getX()) + static_cast<float>(slot) * slotWidth;
        const auto gainY = yForGain(bar.gainDb);
        const auto left = x + 6.0f;
        const auto right = x + slotWidth - 6.0f;
        const auto newest = index == shown - 1;
        const auto boost = bar.gainDb >= 0.0f;
        auto colour = boost ? juce::Colour(0xff6dd3b5) : juce::Colour(0xffe07a4a);
        if (!newest)
            colour = colour.withAlpha(0.72f);
        graphics.setColour(colour);
        if (std::abs(bar.gainDb) < 0.05f)
            graphics.fillRoundedRectangle(left, mid - 1.5f, std::max(2.0f, right - left), 3.0f, 1.0f);
        else
            graphics.fillRect(left, std::min(mid, gainY), std::max(2.0f, right - left), std::abs(gainY - mid));

        graphics.setColour(juce::Colour(0xffd5dbe3));
        const auto caption = (bar.gainDb > 0.05f ? "+" : "") + juce::String(bar.gainDb, 1);
        graphics.drawText(caption,
                          juce::roundToInt(x),
                          captions.getY(),
                          juce::roundToInt(slotWidth),
                          captions.getHeight(),
                          juce::Justification::centred);
    }
}
