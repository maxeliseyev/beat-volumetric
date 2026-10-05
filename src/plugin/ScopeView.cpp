#include "ScopeView.h"

#include <algorithm>
#include <cmath>

void ScopeView::setFrame(const HitScope::Frame& next)
{
    frame = next;
    hasFrame = next.hasSignal;
    repaint();
}

void ScopeView::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds();
    graphics.setColour(juce::Colour(0xff050607));
    graphics.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    graphics.setColour(juce::Colour(0xff1d3a24));
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 8.0f, 1.0f);

    auto plot = bounds.reduced(10, 8);
    auto caption = plot.removeFromBottom(16);
    const auto gap = 8;
    auto inputLane = plot.removeFromTop((plot.getHeight() - gap) / 2);
    plot.removeFromTop(gap);
    auto outputLane = plot;

    auto drawLane = [&](juce::Rectangle<int> lane, const HitScope::Column* columns, const char* name, juce::Colour colour)
    {
        auto label = lane.removeFromLeft(36);
        const auto top = static_cast<float>(lane.getY());
        const auto bottom = static_cast<float>(lane.getBottom());
        const auto mid = (top + bottom) * 0.5f;
        const auto half = (bottom - top) * 0.5f;
        auto yFor = [&](float sample)
        {
            return mid - std::clamp(sample, -1.0f, 1.0f) * half;
        };

        graphics.setColour(juce::Colour(0xff102016));
        for (int division = 1; division < 8; ++division)
        {
            const auto x = lane.getX() + lane.getWidth() * division / 8;
            graphics.drawVerticalLine(x, top, bottom);
        }
        for (const auto mark : { 0.5f, 0.0f, -0.5f })
        {
            graphics.setColour(mark == 0.0f ? juce::Colour(0xff245536) : juce::Colour(0xff102016));
            graphics.drawHorizontalLine(juce::roundToInt(yFor(mark)),
                                        static_cast<float>(lane.getX()),
                                        static_cast<float>(lane.getRight()));
        }

        graphics.setColour(colour.withAlpha(0.9f));
        graphics.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        graphics.drawText(name, label, juce::Justification::centredRight);

        if (!hasFrame)
            return;

        graphics.setColour(colour);
        const auto width = static_cast<float>(lane.getWidth()) / static_cast<float>(HitScope::columns);
        for (std::size_t index = 0; index < HitScope::columns; ++index)
        {
            const auto& column = columns[index];
            if (!column.touched)
                continue;
            const auto x = static_cast<float>(lane.getX()) + static_cast<float>(index) * width;
            const auto y0 = yFor(column.maximum);
            const auto y1 = yFor(column.minimum);
            graphics.fillRect(x, y0, std::max(1.0f, width), std::max(1.0f, y1 - y0));
        }

        if (frame.writeColumn >= 0)
        {
            const auto x = lane.getX()
                           + lane.getWidth() * frame.writeColumn / static_cast<int>(HitScope::columns);
            graphics.setColour(juce::Colour(0xffd7ffe0).withAlpha(0.45f));
            graphics.drawVerticalLine(x, top, bottom);
        }
    };

    drawLane(inputLane, frame.input.data(), "IN", juce::Colour(0xff1f8a38));
    drawLane(outputLane, frame.output.data(), "OUT", juce::Colour(0xff3eea4a));

    if (!hasFrame)
    {
        graphics.setColour(juce::Colour(0xff8fb89a));
        graphics.setFont(juce::FontOptions(13.0f));
        graphics.drawFittedText("Play to fill the last 2 bars",
                                bounds.reduced(20),
                                juce::Justification::centred,
                                2);
    }

    graphics.setColour(juce::Colour(0xffd7ffe0));
    graphics.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const auto captionText = frame.musical
                                 ? juce::String(HitScope::bars) + " BARS"
                                 : juce::String(frame.spanSeconds, 1) + " s";
    graphics.drawText(captionText, caption, juce::Justification::centredLeft);
}
