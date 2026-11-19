#include "FeedbackStage.h"

namespace progdelay
{
void FeedbackStage::prepare (double sr)
{
    sampleRate = sr;
    maxBar = sr * kMaxBarSeconds;
    line.prepare (2, (int64_t) (maxBar + sr * 0.1) + 4 * kMaxChunk);  // + room for modulation
    modes.prepare (sr);
    barX.prepare (sr, 20.0, 300.0);
    fbGain.reset (sr, 0.03);
    lowCutS.reset (sr, 0.05);
    highCutS.reset (sr, 0.05);
    reset();
}

void FeedbackStage::reset() noexcept
{
    line.clear();
    modes.reset();
    for (auto& f : hp) f.reset();
    for (auto& f : lp) f.reset();
    modPrev[0] = modPrev[1] = modCur[0] = modCur[1] = 0.0f;
    first = true;
}

void FeedbackStage::process (const float* s1L, const float* s1R, float* s2L, float* s2R, int n, const Params& p,
                             const float* feedGain) noexcept
{
    jassert (n <= kMaxChunk);
    const double minB = kMaxChunk + 8.0;
    const double B = std::clamp (p.barSamples, minB, maxBar);
    modes.setMode (p.modeIndex);

    if (first)
    {
        barX.reset (B);
        fbGain.setCurrentAndTargetValue (p.feedback);
        lowCutS.setCurrentAndTargetValue (p.lowCutHz);
        highCutS.setCurrentAndTargetValue (p.highCutHz);
        first = false;
    }
    barX.setTarget (B, modes.prefersGlide());
    fbGain.setTargetValue (p.feedback);
    lowCutS.setTargetValue (p.lowCutHz);
    highCutS.setTargetValue (p.highCutHz);

    // filters are updated once per chunk
    const float lc = lowCutS.skip (n), hc = highCutS.skip (n);
    for (int ch = 0; ch < 2; ++ch)
    {
        hp[ch].setup (sampleRate, lc, 0.7071);
        lp[ch].setup (sampleRate, hc, 0.7071);
    }
    const bool useHp = lc > 20.5f, useLp = hc < 19900.0f;

    const int64_t t0 = line.now();
    const float invN = 1.0f / (float) n;

    // 1) read the delayed (s1 + s2), apply feedback gain and stereo mode
    for (int i = 0; i < n; ++i)
    {
        barX.step();
        const float a = barX.alpha();
        const double t = (double) (t0 + i);
        const float frac = (float) (i + 1) * invN;
        float y[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            const double m = modPrev[ch] + (modCur[ch] - modPrev[ch]) * frac;
            const double dCur = std::max (minB - 4.0, barX.current() + m);
            float v = line.read (ch, t - dCur);
            if (a < 1.0f)
            {
                const double dPrev = std::max (minB - 4.0, barX.previous() + m);
                v = a * v + (1.0f - a) * line.read (ch, t - dPrev);
            }
            y[ch] = v;
        }
        if (p.stereo == StereoMode::PingPong)
            std::swap (y[0], y[1]);
        else if (p.stereo == StereoMode::Mono)
            y[0] = y[1] = 0.5f * (y[0] + y[1]);

        const float g = fbGain.getNextValue();
        y[0] *= g;
        y[1] *= g;
        if (useHp)
        {
            y[0] = hp[0].highpass (y[0]);
            y[1] = hp[1].highpass (y[1]);
        }
        if (useLp)
        {
            y[0] = lp[0].lowpass (y[0]);
            y[1] = lp[1].lowpass (y[1]);
        }
        s2L[i] = y[0];
        s2R[i] = y[1];
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        hp[ch].flushDenormals();
        lp[ch].flushDenormals();
    }

    // 2) sound mode inside the loop, then the soft clipper
    modes.process (s2L, s2R, n, p.mode);
    for (int i = 0; i < n; ++i)
    {
        s2L[i] = softClip (s2L[i]);
        s2R[i] = softClip (s2R[i]);
    }

    // 3) write s1 + s2
    for (int i = 0; i < n; ++i)
    {
        const float fg = feedGain != nullptr ? feedGain[i] : 1.0f;
        line.write (0, t0 + i, s1L[i] * fg + s2L[i]);
        line.write (1, t0 + i, s1R[i] * fg + s2R[i]);
    }
    line.advance (n);

    for (int ch = 0; ch < 2; ++ch)
    {
        modPrev[ch] = modCur[ch];
        modCur[ch] = modes.getDelayModulationSamples (ch);
    }
}
} // namespace progdelay
