#include "DuckEngine.h"

#include <algorithm>

namespace subside
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    }

    // -----------------------------------------------------------------------
    // EnvelopeFollower
    // -----------------------------------------------------------------------
    void EnvelopeFollower::prepare (double fs, float attackSec, float releaseSec)
    {
        attackCoeff  = static_cast<float> (1.0 - std::exp (-1.0 / (attackSec * fs)));
        releaseCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (releaseSec * fs)));
        env = 0.0f;
    }

    void EnvelopeFollower::reset() { env = 0.0f; }

    float EnvelopeFollower::process (float x)
    {
        const float a = std::fabs (x);
        env += (a > env ? attackCoeff : releaseCoeff) * (a - env);
        return env;
    }

    // -----------------------------------------------------------------------
    // CurveLane
    // -----------------------------------------------------------------------
    void CurveLane::prepare (double /*fs*/) { reset(); }

    void CurveLane::reset() { phase = 1.0f; }

    void CurveLane::setDuration (float seconds, double fs)
    {
        duration = seconds < 0.01f ? 0.01f : seconds;
        stepPerSample = static_cast<float> (1.0 / (duration * fs));
    }

    float CurveLane::readLut (float ph) const
    {
        if (lut == nullptr)
            return 0.0f;

        const float x = ph * (kCurveLutSize - 1);
        int i = static_cast<int> (x);
        if (i >= kCurveLutSize - 1)
            return lut[kCurveLutSize - 1];

        const float f = x - i;
        return lut[i] + f * (lut[i + 1] - lut[i]);
    }

    float CurveLane::process (bool trigger)
    {
        if (trigger)
            phase = 0.0f;

        if (phase < 1.0f)
        {
            const float v = readLut (phase);
            phase += stepPerSample;
            if (phase >= 1.0f)
                phase = 1.0f;
            return v;
        }
        return 0.0f;
    }

    // -----------------------------------------------------------------------
    // Biquad
    // -----------------------------------------------------------------------
    void Biquad::reset() { z1 = z2 = 0.0f; }

    void Biquad::setLowpass (double fs, float fc)
    {
        const float w0 = 2.0f * kPi * fc / static_cast<float> (fs);
        const float cw = std::cos (w0);
        const float sw = std::sin (w0);
        const float alpha = sw / (2.0f * 0.70710678f);
        const float a0 = 1.0f + alpha;

        b0 = ((1.0f - cw) / 2.0f) / a0;
        b1 = (1.0f - cw) / a0;
        b2 = ((1.0f - cw) / 2.0f) / a0;
        a1 = (-2.0f * cw) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void Biquad::setHighpass (double fs, float fc)
    {
        const float w0 = 2.0f * kPi * fc / static_cast<float> (fs);
        const float cw = std::cos (w0);
        const float sw = std::sin (w0);
        const float alpha = sw / (2.0f * 0.70710678f);
        const float a0 = 1.0f + alpha;

        b0 = ((1.0f + cw) / 2.0f) / a0;
        b1 = -(1.0f + cw) / a0;
        b2 = ((1.0f + cw) / 2.0f) / a0;
        a1 = (-2.0f * cw) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    float Biquad::process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // -----------------------------------------------------------------------
    // LookaheadDelay
    // -----------------------------------------------------------------------
    void LookaheadDelay::prepare (double fs, float maxSeconds)
    {
        buffer.assign (static_cast<size_t> (fs * maxSeconds) + 16, 0.0f);
        writePos = 0;
        delaySamples = 0;
    }

    void LookaheadDelay::setDelaySamples (int samples)
    {
        const int n = static_cast<int> (buffer.size());
        delaySamples = samples < 0 ? 0 : (samples >= n ? n - 1 : samples);
    }

    void LookaheadDelay::reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    float LookaheadDelay::process (float x)
    {
        buffer[static_cast<size_t> (writePos)] = x;
        writePos = (writePos + 1) % static_cast<int> (buffer.size());

        int read = writePos - delaySamples;
        if (read < 0)
            read += static_cast<int> (buffer.size());
        return buffer[static_cast<size_t> (read)];
    }

    // -----------------------------------------------------------------------
    // DuckEngine
    // -----------------------------------------------------------------------
    void DuckEngine::prepare (double sampleRate, int /*maxBlockSize*/)
    {
        fs = sampleRate;

        follower.prepare (fs, 0.001f, 0.05f);

        lane[0].prepare (fs);
        lane[1].prepare (fs);
        lane[0].setLut (lut[0].data());
        lane[1].setLut (lut[1].data());

        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        for (auto& d : lookahead) d.prepare (fs, 0.1f);

        vizMainBuf.assign (kVizSize, 0.0f);
        vizSideBuf.assign (kVizSize, 0.0f);
        vizDuckBuf.assign (kVizSize, 0.0f);
        vizWrite.store (0);

        // Sensible default curves (fast attack -> hold -> release).
        setCurve (0, { { 0.0f, 0.0f }, { 0.06f, 1.0f }, { 0.30f, 0.85f }, { 1.0f, 0.0f } });
        setCurve (1, { { 0.0f, 0.0f }, { 0.12f, 1.0f }, { 0.50f, 0.80f }, { 1.0f, 0.0f } });

        updateCoeffs (fs);
        reset();
    }

    void DuckEngine::reset()
    {
        follower.reset();
        lane[0].reset();
        lane[1].reset();
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        for (auto& d : lookahead) d.reset();

        audioWasAbove = false;
        duckAmount.store (0.0f);
        audioDuck.store (0.0f);
        midiDuck.store (0.0f);
        sidechainLevel.store (0.0f);
        vizWrite.store (0);
    }

    void DuckEngine::setParams (const Params& pp)
    {
        if (pp == p)
            return;   // nothing changed -> skip re-computing crossovers/curve timing
        p = pp;
        updateCoeffs (fs);
    }

    void DuckEngine::updateCoeffs (double sampleRate)
    {
        // Fixed crossovers: low/mid 250 Hz, mid/high 3.5 kHz.
        for (auto& f : lp) f.setLowpass (sampleRate, 250.0f);
        for (auto& f : hp) f.setHighpass (sampleRate, 3500.0f);

        lane[0].setDuration (p.audioTime, sampleRate);
        lane[1].setDuration (p.midiTime, sampleRate);

        const int ls = static_cast<int> (p.lookahead * static_cast<float> (sampleRate));
        for (auto& d : lookahead) d.setDelaySamples (ls);
    }

    void DuckEngine::setCurve (int laneIndex, const std::vector<CurvePoint>& points)
    {
        if (laneIndex < 0 || laneIndex >= kNumLanes)
            return;
        curvePoints[laneIndex] = points;
        rebuildLut (laneIndex, points);
    }

    void DuckEngine::rebuildLut (int laneIndex, const std::vector<CurvePoint>& points)
    {
        auto pts = points;
        std::sort (pts.begin(), pts.end(),
                   [] (const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });

        for (int i = 0; i < kCurveLutSize; ++i)
        {
            const float x = clamp01 (static_cast<float> (i) / (kCurveLutSize - 1));

            float y = 0.0f;
            if (pts.empty())
                y = 0.0f;
            else if (pts.size() == 1)
                y = clamp01 (pts[0].y);
            else if (x <= pts.front().x)
                y = clamp01 (pts.front().y);
            else if (x >= pts.back().x)
                y = clamp01 (pts.back().y);
            else
            {
                for (size_t j = 0; j + 1 < pts.size(); ++j)
                {
                    if (x >= pts[j].x && x <= pts[j + 1].x)
                    {
                        const float x0 = pts[j].x;
                        const float x1 = pts[j + 1].x;
                        const float t = (x1 == x0) ? 0.0f : (x - x0) / (x1 - x0);
                        y = pts[j].y + t * (pts[j + 1].y - pts[j].y);
                        break;
                    }
                }
            }

            lut[laneIndex][static_cast<size_t> (i)] = clamp01 (y);
        }
    }

    void DuckEngine::process (const float* inL, const float* inR,
                              const float* scL, const float* scR,
                              float* outL, float* outR,
                              bool midiTrigger, int numSamples)
    {
        float lastScEnv = 0.0f, lastDuck = 0.0f, lastAd = 0.0f, lastMd = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float il = inL[i];
            const float ir = inR[i];

            // Sidechain envelope (peak of L/R).
            float sc = 0.0f;
            if (scL != nullptr && scR != nullptr)
                sc = std::max (std::fabs (scL[i]), std::fabs (scR[i]));
            else if (scL != nullptr)
                sc = std::fabs (scL[i]);

            const float scEnv = follower.process (sc);
            lastScEnv = scEnv;

            // Audio lane: rising edge of the envelope crossing the threshold.
            const bool above = scEnv > p.threshold;
            const bool audioTrigger = above && !audioWasAbove;
            audioWasAbove = above;

            // MIDI lane: trigger once at the start of the block.
            const bool midiTrig = (i == 0) ? midiTrigger : false;

            const float ad = lane[0].process (audioTrigger);
            const float md = lane[1].process (midiTrig);
            const float duck = std::max (ad, md);

            lastDuck = duck;
            lastAd = ad;
            lastMd = md;

            // Main path: lookahead -> 3-band split -> per-band duck -> sum.
            const float xl = lookahead[0].process (il);
            const float xr = lookahead[1].process (ir);

            const float lowL  = lp[0].process (xl);
            const float highL = hp[0].process (xl);
            const float midL  = xl - lowL - highL;
            const float wetL  = xl - duck * p.depth
                                     * (lowL * p.lowDepth + midL * p.midDepth + highL * p.highDepth);
            outL[i] = ((1.0f - p.mix) * xl + p.mix * wetL) * p.output;

            const float lowR  = lp[1].process (xr);
            const float highR = hp[1].process (xr);
            const float midR  = xr - lowR - highR;
            const float wetR  = xr - duck * p.depth
                                     * (lowR * p.lowDepth + midR * p.midDepth + highR * p.highDepth);
            outR[i] = ((1.0f - p.mix) * xr + p.mix * wetR) * p.output;

            // Visualisation ring buffers.
            int w = vizWrite.load();
            vizMainBuf[static_cast<size_t> (w)] = 0.5f * (il + ir);
            vizSideBuf[static_cast<size_t> (w)] = sc;
            vizDuckBuf[static_cast<size_t> (w)] = duck;
            vizWrite.store ((w + 1) % kVizSize);
        }

        // Publish the live values once per block (the UI polls at ~60 Hz).
        duckAmount.store (lastDuck);
        audioDuck.store (lastAd);
        midiDuck.store (lastMd);
        sidechainLevel.store (lastScEnv);
    }

} // namespace subside
