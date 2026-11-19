#pragma once
// Ducking: lowers the reflections while there is input, so the delay blooms in
// the gaps.
//
// Architecture (kept open for band-limited ducking):
//   DuckDetector   detector filters (HP/LP), stereo-linked Peak/RMS level
//   Ducker         gain computer (threshold, depth, knee) + ballistics
//                  (attack, hold, release) -> one gain value per sample
//   DuckApplier    applies that gain curve to a signal. BroadbandApplier is a
//                  plain multiply; a future BandApplier would split the wet
//                  signal with a crossover and apply the gain to one band only,
//                  without touching the detector or the gain computer.
// The display data (detector level, gain reduction) goes to the UI through a
// lock-free FIFO.

#include <array>
#include <atomic>
#include <juce_core/juce_core.h>
#include "Filters.h"
#include "Parameters.h"

namespace progdelay
{
struct DuckParams
{
    bool on = false;
    float hpHz = 20.0f, lpHz = 20000.0f;
    DetMode mode = DetMode::Peak;
    float rmsMs = 30.0f;
    float threshDb = -24.0f;
    float depthDb = -12.0f;
    bool full = false;          // duck to silence
    float kneeDb = 6.0f;
    float attackMs = 5.0f, holdMs = 50.0f, releaseMs = 250.0f;  // releaseMs already tempo-resolved
};

class DuckDetector
{
public:
    void prepare (double sampleRate);
    void reset();
    /** Linear detector level of one stereo frame (stereo-linked). */
    inline float process (float l, float r, const DuckParams& p) noexcept;
    void update (const DuckParams& p) noexcept;

private:
    double sr = 48000.0;
    Svf hp[2], lp[2];
    float lastHp = -1.0f, lastLp = -1.0f, lastRms = -1.0f;
    double peakEnv = 0.0, peakRel = 0.0, ms = 0.0, msCoef = 0.0;
};

class Ducker
{
public:
    static constexpr float kFloorDb = -80.0f;   // "full duck" target in the dB domain

    void prepare (double sampleRate);
    void reset();

    /** Fills gains[0..n) with the linear duck gain for each sample. detL/detR = detector input. */
    void process (const float* detL, const float* detR, int n, const DuckParams& p, float* gains) noexcept;

    /** Static gain-reduction curve (dB, <= 0) for a detector level in dB. */
    static float targetReductionDb (float levelDb, const DuckParams& p) noexcept;

    float getCurrentReductionDb() const noexcept { return grDb; }

    // ---- display FIFO (audio thread writes, UI reads)
    struct Frame { float levelDb, grDb; };
    int readFrames (Frame* dest, int maxFrames) noexcept;
    std::atomic<float> meterGrDb { 0.0f };

private:
    void pushFrame (float levelDb, float gr) noexcept;

    double sr = 48000.0;
    DuckDetector detector;
    float grDb = 0.0f;
    int holdCounter = 0;

    // display decimation (one frame per ~10 ms)
    int frameLen = 480, frameCount = 0;
    float frameMaxLevel = -120.0f, frameMinGr = 0.0f;
    static constexpr int kFifoSize = 2048;
    juce::AbstractFifo fifo { kFifoSize };
    std::array<Frame, kFifoSize> frames {};
};

/** Applies the duck gain curve to a stereo signal. */
class DuckApplier
{
public:
    virtual ~DuckApplier() = default;
    virtual void prepare (double /*sampleRate*/) {}
    virtual void reset() {}
    virtual void apply (float* L, float* R, const float* gains, int n) noexcept = 0;
};

class BroadbandApplier final : public DuckApplier
{
public:
    void apply (float* L, float* R, const float* gains, int n) noexcept override
    {
        for (int i = 0; i < n; ++i)
        {
            L[i] *= gains[i];
            R[i] *= gains[i];
        }
    }
};

// ---------------------------------------------------------------------------- inline
inline float DuckDetector::process (float l, float r, const DuckParams& p) noexcept
{
    if (p.hpHz > 20.5f)
    {
        l = hp[0].highpass (l);
        r = hp[1].highpass (r);
    }
    if (p.lpHz < 19900.0f)
    {
        l = lp[0].lowpass (l);
        r = lp[1].lowpass (r);
    }
    if (p.mode == DetMode::Rms)
    {
        const double x2 = 0.5 * ((double) l * l + (double) r * r);
        ms += msCoef * (x2 - ms);
        return (float) std::sqrt (std::max (0.0, ms));
    }
    const double a = std::max (std::abs (l), std::abs (r));
    peakEnv = a > peakEnv ? a : peakEnv + peakRel * (a - peakEnv);
    return (float) peakEnv;
}
} // namespace progdelay
