#pragma once

#include "HitScope.h"

#include <juce_gui_basics/juce_gui_basics.h>

class ScopeView final : public juce::Component
{
public:
    void setFrame(const HitScope::Frame& next);
    void paint(juce::Graphics& graphics) override;

private:
    HitScope::Frame frame;
    bool hasFrame = false;
};
