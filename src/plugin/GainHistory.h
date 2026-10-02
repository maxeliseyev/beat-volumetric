#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

// Recent hit gain changes. Bars stay put until newer hits replace them.
class GainHistory final : public juce::Component
{
public:
    struct Bar
    {
        float gainDb = 0.0f;
    };

    void setBars(const Bar* bars, std::size_t count);
    void paint(juce::Graphics&) override;

private:
    std::vector<Bar> bars;
};
