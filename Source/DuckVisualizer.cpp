#include "DuckVisualizer.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    // Builds a scrolling waveform path from a ring buffer (oldest -> newest).
    void buildWaveformPath (juce::Path& p, const std::vector<float>& buf, int writePos,
                            juce::Rectangle<float> b)
    {
        const int n = static_cast<int> (buf.size());
        if (n == 0)
            return;

        const float midY = b.getCentreY();
        const float amp  = b.getHeight() * 0.45f;

        for (int i = 0; i < n; ++i)
        {
            const int idx = (writePos + i) % n;
            const float x = b.getX() + static_cast<float> (i) / (n - 1) * b.getWidth();
            const float y = midY - buf[static_cast<size_t> (idx)] * amp;

            if (i == 0)
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }
    }
}

// ===========================================================================
// LanePanel
// ===========================================================================
LanePanel::LanePanel (SubsideAudioProcessor& p, int laneIndex)
    : proc (p), lane (laneIndex)
{
    startTimerHz (60);
}

LanePanel::~LanePanel()
{
    stopTimer();
}

void LanePanel::timerCallback()
{
    auto& engine = proc.getEngine();
    playhead = (lane == 0) ? engine.getAudioDuck() : engine.getMidiDuck();
    repaint();
}

void LanePanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    g.setColour (SympathyLookAndFeel::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (SympathyLookAndFeel::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    auto& engine = proc.getEngine();
    const bool isAudio = (lane == 0);

    const juce::Colour accent = isAudio
        ? juce::Colour::fromHSV (0.50f, 0.70f, 0.95f, 1.0f)   // cyan
        : juce::Colour::fromHSV (0.75f, 0.65f, 0.95f, 1.0f);  // violet

    auto plot = b.reduced (12.0f, 10.0f);

    // Header
    auto header = plot.removeFromTop (16.0f);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (accent);
    g.drawText (isAudio ? "AUDIO TRIGGER" : "MIDI TRIGGER",
                header.removeFromLeft (120), juce::Justification::centredLeft, false);
    g.setFont (juce::Font (10.0f, juce::Font::plain));
    g.setColour (SympathyLookAndFeel::muted);
    g.drawText ("click + drag to draw  |  double-click to reset",
                header, juce::Justification::centredRight, false);

    // Input waveform (audio lane only).
    if (isAudio)
    {
        juce::Path wave;
        buildWaveformPath (wave, engine.vizSidechain(), engine.vizWritePos(), plot);
        g.setColour (SympathyLookAndFeel::text.withAlpha (0.22f));
        g.strokePath (wave, juce::PathStrokeType (1.0f));
    }

    // Threshold line (audio lane only).
    if (isAudio)
    {
        const float th = proc.getParameters().getRawParameterValue ("threshold")->load();
        const float yTh = plot.getBottom() - th * plot.getHeight();
        g.setColour (SympathyLookAndFeel::accent.withAlpha (0.45f));
        g.drawLine (plot.getX(), yTh, plot.getRight(), yTh, 1.0f);
    }

    // Curve fill + line.
    auto pts = proc.getCurvePoints (lane);
    std::sort (pts.begin(), pts.end(),
               [] (const subside::CurvePoint& a, const subside::CurvePoint& b) { return a.x < b.x; });

    if (!pts.empty())
    {
        auto toPlot = [&] (const subside::CurvePoint& pt) -> juce::Point<float>
        {
            return { plot.getX() + pt.x * plot.getWidth(),
                     plot.getBottom() - pt.y * plot.getHeight() };
        };

        juce::Path fill;
        fill.startNewSubPath (plot.getX(), plot.getBottom());
        for (const auto& pt : pts)
        {
            const auto q = toPlot (pt);
            fill.lineTo (q.x, q.y);
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();
        g.setColour (accent.withAlpha (0.16f));
        g.fillPath (fill);

        juce::Path curve;
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const auto q = toPlot (pts[i]);
            if (i == 0)
                curve.startNewSubPath (q.x, q.y);
            else
                curve.lineTo (q.x, q.y);
        }
        g.setColour (accent.withAlpha (0.30f));
        g.strokePath (curve, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Duck amount (horizontal sweep).
    const float yPh = plot.getBottom() - playhead * plot.getHeight();
    g.setColour (accent.withAlpha (0.22f));
    g.fillRect (plot.getX(), yPh, plot.getWidth(), plot.getBottom() - yPh);
    g.setColour (accent.withAlpha (0.7f));
    g.drawLine (plot.getX(), yPh, plot.getRight(), yPh, 1.2f);

    // Playhead dot on the curve.
    const float phase = engine.getLanePhase (lane);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (plot.getX() + phase * plot.getWidth() - 3.0f, yPh - 3.0f, 6.0f, 6.0f);
}

void LanePanel::mouseDown (const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() >= 2)
    {
        resetCurveToDefault();
        return;
    }
    drawing = true;
    pending.clear();
    addPoint (e);
}

void LanePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (drawing)
        addPoint (e);
}

void LanePanel::mouseUp (const juce::MouseEvent&)
{
    if (drawing)
    {
        drawing = false;
        if (!pending.empty())
            proc.setCurvePoints (lane, pending);
    }
}

void LanePanel::addPoint (const juce::MouseEvent& e)
{
    auto plot = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    plot.removeFromTop (16.0f);

    const float nx = juce::jlimit (0.0f, 1.0f,
        (e.position.x - plot.getX()) / plot.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f,
        1.0f - (e.position.y - plot.getY()) / plot.getHeight());

    pending.push_back ({ nx, ny });
}

void LanePanel::resetCurveToDefault()
{
    if (lane == 0)
        proc.setCurvePoints (0, { { 0.0f, 0.0f }, { 0.06f, 1.0f }, { 0.30f, 0.85f }, { 1.0f, 0.0f } });
    else
        proc.setCurvePoints (1, { { 0.0f, 0.0f }, { 0.12f, 1.0f }, { 0.50f, 0.80f }, { 1.0f, 0.0f } });
}

// ===========================================================================
// MainVisualizer
// ===========================================================================
MainVisualizer::MainVisualizer (SubsideAudioProcessor& p)
    : proc (p)
{
    startTimerHz (60);
}

MainVisualizer::~MainVisualizer()
{
    stopTimer();
}

float MainVisualizer::nextRandom()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float> (rngState >> 8) * (1.0f / 16777216.0f);
}

void MainVisualizer::timerCallback()
{
    time += 1.0f / 60.0f;
    duck = proc.getEngine().getDuckAmount();
    update();
    repaint();
}

void MainVisualizer::update()
{
    const int target = juce::roundToInt (kMaxParticles * (0.15f + 0.85f * duck));
    while (static_cast<int> (particles.size()) < target)
        spawnParticle();

    for (auto& pt : particles)
    {
        pt.life -= 1.0f / 60.0f;
        pt.x += pt.vx;
        pt.y += pt.vy;
        pt.vx *= 0.98f;
        pt.vy *= 0.98f;
    }

    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [] (const Particle& p) { return p.life <= 0.0f; }),
                     particles.end());
}

void MainVisualizer::spawnParticle()
{
    const auto b = getLocalBounds().toFloat();
    Particle pt;
    pt.x = b.getX() + nextRandom() * b.getWidth();
    pt.y = b.getBottom() - 2.0f;
    pt.vx = (nextRandom() - 0.5f) * 1.5f;
    pt.vy = -(0.4f + nextRandom() * 1.4f) * duck;
    pt.life = 0.6f + nextRandom() * 1.4f;
    pt.maxLife = pt.life;
    pt.size = 1.5f + 3.0f * nextRandom();
    pt.hue = std::fmod (0.50f + time * 0.02f + nextRandom() * 0.08f, 1.0f);
    particles.push_back (pt);
}

void MainVisualizer::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    g.setColour (SympathyLookAndFeel::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (SympathyLookAndFeel::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    auto& engine = proc.getEngine();
    auto plot = b.reduced (12.0f, 10.0f);

    // Header
    auto header = plot.removeFromTop (16.0f);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (SympathyLookAndFeel::accent);
    g.drawText ("MAIN SIGNAL", header.removeFromLeft (120), juce::Justification::centredLeft, false);
    g.setFont (juce::Font (10.0f, juce::Font::plain));
    g.setColour (SympathyLookAndFeel::muted);
    g.drawText ("input waveform  +  duck amount", header, juce::Justification::centredRight, false);

    // Duck envelope (filled from the bottom = "how much is being ducked").
    const auto& duckBuf = engine.vizDuck();
    const int n = static_cast<int> (duckBuf.size());
    const int wp = engine.vizWritePos();

    juce::Path duckFill;
    duckFill.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i < n; ++i)
    {
        const int idx = (wp + i) % n;
        const float x = plot.getX() + static_cast<float> (i) / (n - 1) * plot.getWidth();
        const float d = duckBuf[static_cast<size_t> (idx)];
        const float y = plot.getBottom() - d * plot.getHeight();
        duckFill.lineTo (x, y);
    }
    duckFill.lineTo (plot.getRight(), plot.getBottom());
    duckFill.closeSubPath();
    g.setColour (SympathyLookAndFeel::accent.withAlpha (0.22f));
    g.fillPath (duckFill);
    g.setColour (SympathyLookAndFeel::accent.withAlpha (0.8f));
    g.strokePath (duckFill, juce::PathStrokeType (1.4f));

    // Input waveform over the duck region.
    juce::Path wave;
    buildWaveformPath (wave, engine.vizMain(), wp, plot);
    g.setColour (SympathyLookAndFeel::text.withAlpha (0.75f));
    g.strokePath (wave, juce::PathStrokeType (1.2f));

    // Particles.
    for (const auto& pt : particles)
    {
        const float a = juce::jlimit (0.0f, 1.0f, pt.life / pt.maxLife);
        const juce::Colour c = juce::Colour::fromHSV (pt.hue, 0.8f, 0.95f, a);
        g.setColour (c);
        g.fillEllipse (pt.x - pt.size * 0.5f, pt.y - pt.size * 0.5f, pt.size, pt.size);
    }
}
