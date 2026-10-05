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
    auto caption = bounds.removeFromBottom(18);
    const auto gap = 14;
    auto inputPanel = bounds.removeFromTop((bounds.getHeight() - gap) / 2);
    bounds.removeFromTop(gap);
    auto outputPanel = bounds;

    const auto inputColour = juce::Colour(0xff6f8f7a);
    const auto outputColour = juce::Colour(0xffffc233);
    auto drawPanel = [&](juce::Rectangle<int> panel, juce::Colour accent, const char* title)
    {
        graphics.setColour(juce::Colour(0xff050607));
        graphics.fillRoundedRectangle(panel.toFloat(), 8.0f);
        graphics.setColour(accent.withAlpha(0.55f));
        graphics.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 8.0f, 1.5f);
        graphics.setColour(accent);
        graphics.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        graphics.drawText(title, panel.reduced(12, 6).removeFromTop(16), juce::Justification::centredLeft);
        return panel.reduced(10, 8).withTrimmedTop(14);
    };
    auto inputLane = drawPanel(inputPanel, inputColour, "INPUT");
    auto outputLane = drawPanel(outputPanel, outputColour, "OUTPUT  ·  AFTER LEVELING");

    auto drawLane = [&](juce::Rectangle<int> lane, const HitScope::Column* columns, juce::Colour colour)
    {
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
            graphics.setColour(juce::Colour(0xfff4f7fb).withAlpha(0.4f));
            graphics.drawVerticalLine(x, top, bottom);
        }
    };

    drawLane(inputLane, frame.input.data(), inputColour);
    drawLane(outputLane, frame.output.data(), outputColour);

    if (!hasFrame)
    {
        graphics.setColour(juce::Colour(0xff8fb89a));
        graphics.setFont(juce::FontOptions(13.0f));
        graphics.drawFittedText("Play to fill the last 2 bars",
                                getLocalBounds().reduced(20),
                                juce::Justification::centred,
                                2);
    }

    graphics.setColour(juce::Colour(0xffa7b0bd));
    graphics.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const auto captionText = frame.musical
                                 ? juce::String(HitScope::bars) + " BARS"
                                 : juce::String(frame.spanSeconds, 1) + " s";
    graphics.drawText(captionText, caption, juce::Justification::centredLeft);
}
