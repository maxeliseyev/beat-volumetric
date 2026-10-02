#include "WaveformScope.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

void WaveformScope::setWaveform(const float* samples, std::size_t count)
{
    if (samples == nullptr || count == 0)
        waveform.clear();
    else
        waveform.assign(samples, samples + count);
    repaint();
}

void WaveformScope::setHits(const Hit* marks, std::size_t count)
{
    if (marks == nullptr || count == 0)
        hits.clear();
    else
        hits.assign(marks, marks + count);
    repaint();
}

void WaveformScope::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds();
    graphics.setColour(juce::Colour(0xff090b0f));
    graphics.fillRoundedRectangle(bounds.toFloat(), 7.0f);
    graphics.setColour(juce::Colour(0xff364452));
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 7.0f, 1.0f);

    auto plot = bounds;
    plot.removeFromTop(22);
    const auto mid = static_cast<float>(plot.getCentreY());
    const auto scale = static_cast<float>(plot.getHeight()) * 0.42f;
    graphics.setColour(juce::Colour(0xff2b3743));
    graphics.drawHorizontalLine(plot.getCentreY(),
                                static_cast<float>(plot.getX()),
                                static_cast<float>(plot.getRight()));
    for (const auto amplitude : { 0.25f, 0.5f })
    {
        const auto offset = amplitude * scale;
        graphics.drawHorizontalLine(juce::roundToInt(mid - offset),
                                    static_cast<float>(plot.getX()),
                                    static_cast<float>(plot.getRight()));
        graphics.drawHorizontalLine(juce::roundToInt(mid + offset),
                                    static_cast<float>(plot.getX()),
                                    static_cast<float>(plot.getRight()));
    }

    if (!waveform.empty())
    {
        for (int x = 0; x < plot.getWidth(); ++x)
        {
            const auto from = static_cast<std::size_t>(x) * waveform.size()
                              / static_cast<std::size_t>(plot.getWidth());
            const auto to = std::max(from + 1, static_cast<std::size_t>(x + 1) * waveform.size()
                                                  / static_cast<std::size_t>(plot.getWidth()));
            float low = 1.0f;
            float high = -1.0f;
            for (auto index = from; index < std::min(to, waveform.size()); ++index)
            {
                low = std::min(low, waveform[index]);
                high = std::max(high, waveform[index]);
            }
            const auto clipped = high > 1.0f || low < -1.0f;
            graphics.setColour(clipped ? juce::Colour(0xffe05d5d) : juce::Colour(0xff5ec8ff));
            graphics.drawVerticalLine(plot.getX() + x,
                                      mid - high * scale,
                                      mid - low * scale);
        }
    }

    auto levelY = [&](float decibels)
    {
        const auto linear = std::pow(10.0f, std::clamp(decibels, -60.0f, 6.0f) / 20.0f);
        const auto y = mid - linear * scale;
        return std::clamp(y, static_cast<float>(plot.getY() + 4), static_cast<float>(plot.getBottom() - 6));
    };

    for (const auto& hit : hits)
    {
        if (hit.position < 0.0f || hit.position > 1.0f)
            continue;
        const auto x = plot.getX()
                       + juce::roundToInt(hit.position * static_cast<float>(std::max(1, plot.getWidth() - 1)));
        const auto measuredY = levelY(hit.measuredDb);
        const auto resultY = levelY(hit.resultDb);
        graphics.setColour(juce::Colour(0xffe8c547).withAlpha(0.85f));
        graphics.drawVerticalLine(x, std::min(measuredY, resultY), std::max(measuredY, resultY));
        graphics.fillEllipse(static_cast<float>(x) - 3.5f, measuredY - 3.5f, 7.0f, 7.0f);
        graphics.setColour(juce::Colour(0xff6dd3b5));
        graphics.drawEllipse(static_cast<float>(x) - 5.0f, resultY - 5.0f, 10.0f, 10.0f, 1.5f);
    }

    graphics.setFont(juce::FontOptions(11.0f));
    graphics.setColour(juce::Colour(0xffe8c547));
    graphics.fillEllipse(10.0f, 8.0f, 7.0f, 7.0f);
    graphics.setColour(juce::Colour(0xffa7b0bd));
    graphics.drawText("before", 20, 4, 46, 14, juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(0xff6dd3b5));
    graphics.drawEllipse(70.0f, 7.0f, 9.0f, 9.0f, 1.5f);
    graphics.setColour(juce::Colour(0xffa7b0bd));
    graphics.drawText("after", 82, 4, 40, 14, juce::Justification::centredLeft);
}
