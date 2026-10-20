#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

#include <vector>

// One trigger lane (Audio / MIDI): a live oscilloscope of its input plus a
// user-drawable duck curve with a playhead showing the current duck amount.
class LanePanel : public juce::Component,
                  private juce::Timer
{
public:
    LanePanel (SubsideAudioProcessor& p, int laneIndex);
    ~LanePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Point<float> mouseToNormalised (const juce::MouseEvent&) const;
    int  findNearestPoint (juce::Point<float>) const;
    void resetCurveToDefault();

    SubsideAudioProcessor& proc;
    int lane = 0;                      // 0 = audio, 1 = MIDI
    float playhead = 0.0f;
    int draggedPoint = -1;
    juce::ToggleButton drawButton { "DRAW" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LanePanel)
};

// The main signal oscilloscope with the combined duck envelope overlaid, plus
// holographic particles that react to the ducking.
class MainVisualizer : public juce::Component,
                       private juce::Timer
{
public:
    explicit MainVisualizer (SubsideAudioProcessor& p);
    ~MainVisualizer() override;

    void paint (juce::Graphics&) override;
    void timerCallback() override;

private:
    struct Particle
    {
        float x, y, vx, vy, life, maxLife, size, hue;
    };

    void update();
    void spawnParticle();
    float nextRandom();

    SubsideAudioProcessor& proc;

    std::vector<Particle> particles;
    uint32_t rngState = 0xABCD1234u;
    float time = 0.0f;
    float duck = 0.0f;

    static constexpr int kMaxParticles = 220;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainVisualizer)
};
