#pragma once

/*
  Subside - sidechain ducking engine (JUCE-free, self-contained).

  Two trigger lanes duck one main signal:
    - Lane 0 "Audio": the sidechain input's envelope crossing a threshold plays
      a user-drawn one-shot duck curve.
    - Lane 1 "MIDI":  each MIDI note-on plays its own user-drawn duck curve.

  Each drawn curve is a normalized polyline (x = time 0..1, y = duck 0..1) that
  is resampled into a fixed lookup table and played back over a per-lane
  "time" (duration). The main signal is split into three bands and each band is
  ducked by an independent depth, then recombined.
*/

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace subside
{
    static constexpr int kCurveLutSize = 256;
    static constexpr int kVizSize       = 4096;
    static constexpr int kNumLanes      = 2;

    struct CurvePoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Params
    {
        float threshold  = 0.10f;   // 0..1  sidechain trigger level
        float audioTime  = 0.80f;   // s     audio lane curve duration
        float midiTime   = 1.00f;   // s     midi  lane curve duration
        float depth      = 0.90f;   // 0..1  overall duck amount
        float lowDepth   = 1.00f;   // 0..1  low  band duck amount
        float midDepth   = 0.80f;   // 0..1  mid  band duck amount
        float highDepth  = 0.60f;   // 0..1  high band duck amount
        float lookahead  = 0.005f;  // s     latency / click-free attack
        float mix        = 1.00f;   // 0..1  dry/wet
        float output     = 1.00f;   // 0..2

        bool operator== (const Params& o) const
        {
            return threshold  == o.threshold
                && audioTime  == o.audioTime
                && midiTime   == o.midiTime
                && depth      == o.depth
                && lowDepth   == o.lowDepth
                && midDepth   == o.midDepth
                && highDepth  == o.highDepth
                && lookahead  == o.lookahead
                && mix        == o.mix
                && output     == o.output;
        }
    };

    // A one-pole envelope follower.
    class EnvelopeFollower
    {
    public:
        void prepare (double fs, float attackSec, float releaseSec);
        void reset();
        float process (float x);

    private:
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float env = 0.0f;
    };

    // Plays a one-shot curve from its LUT on each rising trigger edge.
    class CurveLane
    {
    public:
        void prepare (double fs);
        void reset();
        void setDuration (float seconds, double fs);
        void setLut (const float* lutData) { lut = lutData; }
        float process (bool trigger);
        float getPhase() const { return phase; }

    private:
        float readLut (float phase) const;

        float phase = 1.0f;   // 1.0 = idle
        float duration = 0.8f;
        float stepPerSample = 0.0f;
        const float* lut = nullptr;
    };

    // RBJ biquad used as a crossover (Butterworth Q).
    class Biquad
    {
    public:
        void setLowpass (double fs, float fc);
        void setHighpass (double fs, float fc);
        void reset();
        float process (float x);

    private:
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;
    };

    // A simple ring-buffer delay for lookahead.
    class LookaheadDelay
    {
    public:
        void prepare (double fs, float maxSeconds);
        void setDelaySamples (int samples);
        void reset();
        float process (float x);

    private:
        std::vector<float> buffer;
        int writePos = 0;
        int delaySamples = 0;
    };

    // A compact lookahead peak limiter (2 ms) with a hard-clip safety net.
    class StereoLimiter
    {
    public:
        void prepare (double sr)
        {
            lookahead = static_cast<int> (sr * 0.002);
            if (lookahead < 1) lookahead = 1;
            delayL.assign (static_cast<size_t> (lookahead + 1), 0.0f);
            delayR.assign (static_cast<size_t> (lookahead + 1), 0.0f);
            threshold = 0.89f;
            releaseCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.05 * sr)));
            reset();
        }

        void reset()
        {
            std::fill (delayL.begin(), delayL.end(), 0.0f);
            std::fill (delayR.begin(), delayR.end(), 0.0f);
            writePos = 0;
            gain = 1.0f;
            peak = 0.0f;
        }

        void process (float& l, float& r)
        {
            delayL[static_cast<size_t> (writePos)] = l;
            delayR[static_cast<size_t> (writePos)] = r;

            const float a = std::max (std::fabs (l), std::fabs (r));
            if (a > peak)      peak = a;
            else               peak += releaseCoeff * (a - peak);

            const float target = (peak > threshold) ? threshold / peak : 1.0f;
            if (target < gain) gain = target;
            else               gain += releaseCoeff * (target - gain);

            const int rd = (writePos + 1) % (lookahead + 1);
            l = std::clamp (delayL[static_cast<size_t> (rd)] * gain, -1.0f, 1.0f);
            r = std::clamp (delayR[static_cast<size_t> (rd)] * gain, -1.0f, 1.0f);
            writePos = rd;
        }

    private:
        int lookahead = 96;
        std::vector<float> delayL;
        std::vector<float> delayR;
        int writePos = 0;
        float gain = 1.0f;
        float peak = 0.0f;
        float threshold = 0.89f;
        float releaseCoeff = 0.0f;
    };

    class DuckEngine
    {
    public:
        void prepare (double fs, int maxBlockSize);
        void reset();
        void setParams (const Params& p);
        void setCurve (int lane, const std::vector<CurvePoint>& points);

        void process (const float* inL, const float* inR,
                      const float* scL, const float* scR,
                      float* outL, float* outR,
                      bool midiTrigger, int numSamples);

        // --- live values for the UI ---------------------------------------
        float getDuckAmount() const      { return duckAmount.load(); }
        float getAudioDuck() const       { return audioDuck.load(); }
        float getMidiDuck() const        { return midiDuck.load(); }
        float getSidechainLevel() const  { return sidechainLevel.load(); }
        float getLanePhase (int l) const { return (l >= 0 && l < kNumLanes) ? lane[l].getPhase() : 1.0f; }

        // Ring buffers of recent samples for the oscilloscopes.
        const std::vector<float>& vizMain() const       { return vizMainBuf; }
        const std::vector<float>& vizSidechain() const  { return vizSideBuf; }
        const std::vector<float>& vizDuck() const       { return vizDuckBuf; }
        int  vizWritePos() const                        { return vizWrite.load(); }

    private:
        void updateCoeffs (double fs);
        void rebuildLut (int lane, const std::vector<CurvePoint>& points);

        double fs = 48000.0;
        Params p;

        EnvelopeFollower follower;
        CurveLane lane[kNumLanes];
        Biquad lp[2];
        Biquad hp[2];
        LookaheadDelay lookahead[2];
        StereoLimiter limiter;

        std::array<float, kCurveLutSize> lut[kNumLanes];
        std::array<std::vector<CurvePoint>, kNumLanes> curvePoints;

        float lookaheadSamples = 0.0f;

        // Ring buffers + atomics for the UI.
        std::vector<float> vizMainBuf;
        std::vector<float> vizSideBuf;
        std::vector<float> vizDuckBuf;
        std::atomic<int> vizWrite { 0 };

        std::atomic<float> duckAmount { 0.0f };
        std::atomic<float> audioDuck { 0.0f };
        std::atomic<float> midiDuck { 0.0f };
        std::atomic<float> sidechainLevel { 0.0f };

        bool audioWasAbove = false;
    };

} // namespace subside
