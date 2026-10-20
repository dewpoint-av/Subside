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
    void timerCallback() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void addPoint (const juce::MouseEvent&);
    void resetCurveToDefault();

    SubsideAudioProcessor& proc;
    int lane = 0;                      // 0 = audio, 1 = MIDI
    bool drawing = false;
    std::vector<subside::CurvePoint> pending;
    float playhead = 0.0f;

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

    static constexpr int kMaxParticles = 140;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainVisualizer)
};
