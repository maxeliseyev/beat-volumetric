#include "WaveformScope.h"
#include <algorithm>
#include <cmath>
void WaveformScope::setWaveform(const float* samples, std::size_t count) { waveform.assign(samples, samples + count); repaint(); }
void WaveformScope::paint(juce::Graphics& g)
{
    auto b = getLocalBounds(); g.setColour(juce::Colour(0xff090b0f)); g.fillRoundedRectangle(b.toFloat(), 7.0f); g.setColour(juce::Colour(0xff364452)); g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 7.0f, 1.0f);
    const auto mid = static_cast<float>(b.getCentreY()); g.setColour(juce::Colour(0xff2b3743)); g.drawHorizontalLine(b.getCentreY(), static_cast<float>(b.getX()), static_cast<float>(b.getRight())); if (waveform.empty()) return;
    float peak = 0.08f; for (auto sample : waveform) peak = std::max(peak, std::abs(sample)); const auto scale = static_cast<float>(b.getHeight()) * 0.42f / peak;
    for (int x = 0; x < b.getWidth(); ++x) { const auto from = static_cast<std::size_t>(x) * waveform.size() / static_cast<std::size_t>(b.getWidth()); const auto to = std::max(from + 1, static_cast<std::size_t>(x + 1) * waveform.size() / static_cast<std::size_t>(b.getWidth())); float lo = 1.0f, hi = -1.0f; for (auto i = from; i < std::min(to, waveform.size()); ++i) { lo = std::min(lo, waveform[i]); hi = std::max(hi, waveform[i]); } g.setColour((hi > 1.0f || lo < -1.0f) ? juce::Colour(0xffe05d5d) : juce::Colour(0xff5ec8ff)); g.drawVerticalLine(b.getX() + x, mid - hi * scale, mid - lo * scale); }
}
