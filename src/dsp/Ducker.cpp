#include "Ducker.h"

namespace progdelay
{
// ============================================================================ DuckDetector
void DuckDetector::prepare (double sampleRate)
{
    sr = sampleRate;
    // the peak envelope decays over ~20 ms, enough to bridge zero crossings down to ~25 Hz
    peakRel = 1.0 - std::exp (-1.0 / (sr * 0.02));
    lastHp = lastLp = lastRms = -1.0f;
    reset();
}

void DuckDetector::reset()
{
    for (auto& f : hp) f.reset();
    for (auto& f : lp) f.reset();
    peakEnv = 0.0;
    ms = 0.0;
}

void DuckDetector::update (const DuckParams& p) noexcept
{
    if (p.hpHz != lastHp)
    {
        for (auto& f : hp) f.setup (sr, p.hpHz);
        lastHp = p.hpHz;
    }
    if (p.lpHz != lastLp)
    {
        for (auto& f : lp) f.setup (sr, p.lpHz);
        lastLp = p.lpHz;
    }
    if (p.rmsMs != lastRms)
    {
        msCoef = 1.0 - std::exp (-1.0 / (sr * p.rmsMs * 0.001));
        lastRms = p.rmsMs;
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        hp[ch].flushDenormals();
        lp[ch].flushDenormals();
    }
    if (ms < 1.0e-20) ms = 0.0;
    if (peakEnv < 1.0e-20) peakEnv = 0.0;
}

// ============================================================================ Ducker
void Ducker::prepare (double sampleRate)
{
    sr = sampleRate;
    detector.prepare (sampleRate);
    frameLen = std::max (1, (int) (sampleRate * 0.01));
    reset();
}

void Ducker::reset()
{
    detector.reset();
    grDb = 0.0f;
    holdCounter = 0;
    frameCount = 0;
    frameMaxLevel = -120.0f;
    frameMinGr = 0.0f;
    meterGrDb.store (0.0f);
}

float Ducker::targetReductionDb (float levelDb, const DuckParams& p) noexcept
{
    if (! p.on)
        return 0.0f;
    const float depth = p.full ? kFloorDb : std::clamp (p.depthDb, kFloorDb, 0.0f);
    float amount;
    if (p.kneeDb <= 0.01f)
        amount = levelDb > p.threshDb ? 1.0f : 0.0f;
    else
    {
        const float x = std::clamp ((levelDb - (p.threshDb - 0.5f * p.kneeDb)) / p.kneeDb, 0.0f, 1.0f);
        amount = x * x * (3.0f - 2.0f * x);  // smoothstep through the knee
    }
    return depth * amount;
}

void Ducker::process (const float* detL, const float* detR, int n, const DuckParams& p, float* gains) noexcept
{
    detector.update (p);
    const float attCoef = 1.0f - (float) std::exp (-1.0 / (sr * std::max (0.05f, p.attackMs) * 0.001));
    const float relCoef = 1.0f - (float) std::exp (-1.0 / (sr * std::max (1.0f, p.releaseMs) * 0.001));
    const int holdSamples = (int) (sr * p.holdMs * 0.001);

    for (int i = 0; i < n; ++i)
    {
        const float level = detector.process (detL[i], detR[i], p);
        const float levelDb = level > 1.0e-6f ? 20.0f * std::log10 (level) : -120.0f;
        const float target = targetReductionDb (levelDb, p);

        if (target < grDb)
        {
            grDb += (target - grDb) * attCoef;   // more reduction: attack
            holdCounter = holdSamples;
        }
        else if (holdCounter > 0)
            --holdCounter;
        else
            grDb += (target - grDb) * relCoef;   // recovering: release

        if (grDb > -1.0e-4f)
            grDb = 0.0f;
        gains[i] = grDb <= kFloorDb + 0.5f ? 0.0f : std::pow (10.0f, grDb * 0.05f);

        frameMaxLevel = std::max (frameMaxLevel, levelDb);
        frameMinGr = std::min (frameMinGr, grDb);
        if (++frameCount >= frameLen)
        {
            pushFrame (frameMaxLevel, frameMinGr);
            frameCount = 0;
            frameMaxLevel = -120.0f;
            frameMinGr = 0.0f;
        }
    }
    meterGrDb.store (grDb, std::memory_order_relaxed);
}

void Ducker::pushFrame (float levelDb, float gr) noexcept
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
        frames[(size_t) scope.startIndex1] = { levelDb, gr };
    else if (scope.blockSize2 > 0)
        frames[(size_t) scope.startIndex2] = { levelDb, gr };
    // FIFO full (no editor open): the frame is dropped
}

int Ducker::readFrames (Frame* dest, int maxFrames) noexcept
{
    const auto scope = fifo.read (std::min (maxFrames, fifo.getNumReady()));
    int k = 0;
    for (int i = 0; i < scope.blockSize1; ++i)
        dest[k++] = frames[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i)
        dest[k++] = frames[(size_t) (scope.startIndex2 + i)];
    return k;
}
} // namespace progdelay
