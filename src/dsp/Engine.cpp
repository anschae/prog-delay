#include "Engine.h"

namespace progdelay
{
Engine::Engine()
{
    banks[0] = std::make_unique<BankSnapshot>();
    banks[1] = std::make_unique<BankSnapshot>();
    for (auto& t : tapLevels)
        t.store (0.0f);
}

Engine::~Engine() = default;

void Engine::prepare (double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    stage1.prepare (sr);
    stage2.prepare (sr);
    colorHost.prepare (sr);
    for (auto* v : { &dryGain, &wetGain, &s1Gain, &s2Gain, &widthS, &colorMix })
        v->reset (sr, 0.02);
    reset();
}

void Engine::reset()
{
    stage1.reset();
    stage2.reset();
    colorHost.reset();
    switcher.reset (stage1.getActivePattern());
    if (! bankInitialised)
    {
        acquireBank();
    }
}

void Engine::acquireBank() noexcept
{
    if (! bankBuffer.acquire() && bankInitialised)
        return;
    const int next = bankIndex ^ 1;
    *banks[next] = bankBuffer.readBuffer();
    stage1.setSnapshot (banks[next].get(), bankInitialised);
    bankIndex = next;
    bankInitialised = true;
}

void Engine::process (juce::AudioBuffer<float>& buffer, int numIn, const HostTiming& host,
                      const Settings& s, std::optional<int> midiPattern) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    // ---- tempo
    clock.update (host, s.tempoMode == TempoMode::Manual, s.manualBpm);
    const double spw = samplesPerWhole (clock.getBpm(), sampleRate);
    colorHost.setMode (s.modeIndex);
    stage1.setSamplesPerWhole (spw, s.colorRefl && colorHost.prefersGlide());

    FeedbackStage::Params fp;
    fp.feedback = juce::jlimit (0.0f, 1.0f, s.feedback);
    const int barT = s.fbBarMode == FbBarMode::Manual ? barTicks (s.fbNum, s.fbDen)
                                                      : barTicks (clock.getSigNum(), clock.getSigDen());
    fp.barSamples = barT * spw / TICKS_PER_WHOLE;
    lastBarSamples = fp.barSamples;
    fp.stereo = s.stereo;
    fp.lowCutHz = s.lowCut;
    fp.highCutHz = s.highCut;
    fp.modeIndex = s.modeIndex;
    fp.mode.character = s.character;
    fp.mode.age = s.age;
    fp.mode.tone = s.tone;
    fp.mode.bpm = clock.getBpm();

    // ---- pattern edits
    if (! stage1.isSnapshotFading())
        acquireBank();

    // ---- pattern switching
    PatternSwitcher::Input si;
    si.source = s.switchSource;
    si.quant = s.switchQuant;
    si.automationPattern = s.activePattern;
    si.fallbackPattern = s.fallbackPattern;
    si.midiPattern = midiPattern;
    const auto switchAt = switcher.process (si, clock, *banks[bankIndex], stage1.getActivePattern(), sampleRate, numSamples);
    if (switchAt.has_value())
        stage1.switchTo (switcher.getTarget(), stage1.now() + *switchAt);

    // ---- gains
    const float mix = juce::jlimit (0.0f, 1.0f, s.mix);
    const float dryAmt = std::min (1.0f, 2.0f * (1.0f - mix));
    const float wetAmt = std::min (1.0f, 2.0f * mix);
    dryGain.setTargetValue (s.killDry ? 0.0f : dryAmt * dbToGainWithFloor (s.dryDb));
    wetGain.setTargetValue (wetAmt * dbToGainWithFloor (s.wetDb));
    s1Gain.setTargetValue (dbToGainWithFloor (s.s1Db));
    s2Gain.setTargetValue (dbToGainWithFloor (s.s2Db));
    colorMix.setTargetValue (s.colorRefl ? 1.0f : 0.0f);
    widthS.setTargetValue (s.width);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    const bool monoIn = numIn < 2;
    float peakL = 0.0f, peakR = 0.0f;

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int n = std::min (kChunk, numSamples - start);
        float* L = outL + start;
        float* R = outR + start;

        for (int i = 0; i < n; ++i)
        {
            if (monoIn)
                R[i] = L[i];
            mono[(size_t) i] = 0.5f * (L[i] + R[i]);
        }

        stage1.process (mono.data(), s1L.data(), s1R.data(), n);

        // "Color reflections": the sound mode on stage 1 too (crossfaded on/off)
        if (colorMix.getCurrentValue() > 0.0f || colorMix.isSmoothing())
        {
            std::copy (s1L.begin(), s1L.begin() + n, colL.begin());
            std::copy (s1R.begin(), s1R.begin() + n, colR.begin());
            colorHost.process (colL.data(), colR.data(), n, fp.mode);
            for (int i = 0; i < n; ++i)
            {
                const float c = colorMix.getNextValue();
                s1L[(size_t) i] += c * (colL[(size_t) i] - s1L[(size_t) i]);
                s1R[(size_t) i] += c * (colR[(size_t) i] - s1R[(size_t) i]);
            }
        }

        stage2.process (s1L.data(), s1R.data(), s2L.data(), s2R.data(), n, fp);

        for (int i = 0; i < n; ++i)
        {
            const float g1 = s1Gain.getNextValue();
            const float g2 = s2Gain.getNextValue();
            float wl = s1L[(size_t) i] * g1 + s2L[(size_t) i] * g2;
            float wr = s1R[(size_t) i] * g1 + s2R[(size_t) i] * g2;

            const float w = widthS.getNextValue();
            const float mid = 0.5f * (wl + wr);
            const float side = 0.5f * (wl - wr) * w;
            wl = mid + side;
            wr = mid - side;

            const float dg = dryGain.getNextValue();
            const float wg = wetGain.getNextValue();
            L[i] = L[i] * dg + wl * wg;
            R[i] = R[i] * dg + wr * wg;
            peakL = std::max (peakL, std::abs (L[i]));
            peakR = std::max (peakR, std::abs (R[i]));
        }
    }

    // ---- read-outs
    playingPattern.store (stage1.getActivePattern(), std::memory_order_relaxed);
    pendingPattern.store (switcher.getPending().value_or (-1), std::memory_order_relaxed);
    bpmDisplay.store ((float) clock.getBpm(), std::memory_order_relaxed);
    sigNumDisplay.store (clock.getSigNum(), std::memory_order_relaxed);
    sigDenDisplay.store (clock.getSigDen(), std::memory_order_relaxed);
    hostPlaying.store (clock.isPlaying(), std::memory_order_relaxed);
    if (clock.getPpq().has_value())
        ppqInBar.store (*clock.getPpq() - clock.getPpqBarStart().value_or (0.0), std::memory_order_relaxed);
    outPeakL.store (std::max (peakL, outPeakL.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    outPeakR.store (std::max (peakR, outPeakR.load (std::memory_order_relaxed)), std::memory_order_relaxed);

    stage1.collectTapLevels (tapScratch);
    for (size_t i = 0; i < tapScratch.size(); ++i)
        if (tapScratch[i] > tapLevels[i].load (std::memory_order_relaxed))
            tapLevels[i].store (tapScratch[i], std::memory_order_relaxed);
}
} // namespace progdelay
