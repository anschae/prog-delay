#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "model/StateSerializer.h"

namespace progdelay
{
ProgDelayProcessor::ProgDelayProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pMix = raw (pid::mix);
    pDry = raw (pid::dry);
    pWet = raw (pid::wet);
    pKillDry = raw (pid::killDry);
    pWidth = raw (pid::width);
    pS1 = raw (pid::s1Level);
    pS2 = raw (pid::s2Level);
    pTempoMode = raw (pid::tempoMode);
    pManualBpm = raw (pid::manualBpm);
    pActive = raw (pid::activePattern);
    pSwitchSource = raw (pid::switchSource);
    pSwitchQuant = raw (pid::switchQuant);
    pFallback = raw (pid::fallback);
    pFeedback = raw (pid::feedback);
    c_age = raw (pid::age);
    c_character = raw (pid::character);
    c_colorRefl = raw (pid::colorRefl);
    c_fbBarDen = raw (pid::fbBarDen);
    c_fbBarMode = raw (pid::fbBarMode);
    c_fbBarNum = raw (pid::fbBarNum);
    c_highCut = raw (pid::highCut);
    c_lowCut = raw (pid::lowCut);
    c_soundMode = raw (pid::soundMode);
    c_stereoMode = raw (pid::stereoMode);
    c_tone = raw (pid::tone);

    d_on = raw (pid::duckOn); d_source = raw (pid::duckSource); d_hp = raw (pid::duckHP); d_lp = raw (pid::duckLP);
    d_det = raw (pid::duckDetMode); d_rms = raw (pid::duckRms); d_thresh = raw (pid::duckThresh);
    d_depth = raw (pid::duckDepth); d_full = raw (pid::duckFull); d_knee = raw (pid::duckKnee);
    d_attack = raw (pid::duckAttack); d_hold = raw (pid::duckHold); d_release = raw (pid::duckRelease);
    d_relSync = raw (pid::duckRelSync); d_relNote = raw (pid::duckRelNote); d_look = raw (pid::duckLook);
    d_target = raw (pid::duckTarget); d_pos = raw (pid::duckPos);

    bank.getTree().addListener (this);
    publishPatterns();
    startTimerHz (10);
}

ProgDelayProcessor::~ProgDelayProcessor()
{
    stopTimer();
    bank.getTree().removeListener (this);
}

void ProgDelayProcessor::publishPatterns()
{
    const juce::SpinLock::ScopedLockType lock (publishLock);  // single writer
    auto& tb = engine.getBankBuffer();
    bank.fillSnapshot (tb.writeBuffer());
    tb.publish();
}

bool ProgDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void ProgDelayProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (Engine::lookaheadSamplesFor (d_look->load(), sampleRate));
}

void ProgDelayProcessor::timerCallback()
{
    const int lat = Engine::lookaheadSamplesFor (d_look->load(), currentSampleRate);
    if (lat != getLatencySamples())
        setLatencySamples (lat);
}

void ProgDelayProcessor::reset()
{
    engine.reset();
}

Engine::Settings ProgDelayProcessor::readSettings() const noexcept
{
    Engine::Settings s;
    s.tempoMode = (TempoMode) juce::roundToInt (pTempoMode->load());
    s.manualBpm = pManualBpm->load();
    s.mix = pMix->load() * 0.01f;
    s.dryDb = pDry->load();
    s.wetDb = pWet->load();
    s.killDry = pKillDry->load() > 0.5f;
    s.width = pWidth->load() * 0.01f;
    s.s1Db = pS1->load();
    s.s2Db = pS2->load();
    s.activePattern = juce::roundToInt (pActive->load()) - 1;
    s.switchSource = (SwitchSource) juce::roundToInt (pSwitchSource->load());
    s.switchQuant = (SwitchQuant) juce::roundToInt (pSwitchQuant->load());
    s.fallbackPattern = juce::roundToInt (pFallback->load()) - 1;
    s.feedback = pFeedback->load() * 0.01f;
    s.fbBarMode = (FbBarMode) juce::roundToInt (c_fbBarMode->load());
    s.fbNum = juce::roundToInt (c_fbBarNum->load());
    s.fbDen = kFbDenominators[juce::jlimit (0, 3, juce::roundToInt (c_fbBarDen->load()))];
    s.lowCut = c_lowCut->load();
    s.highCut = c_highCut->load();
    s.stereo = (StereoMode) juce::roundToInt (c_stereoMode->load());
    s.modeIndex = juce::roundToInt (c_soundMode->load());
    s.character = c_character->load() * 0.01f;
    s.age = c_age->load() * 0.01f;
    s.tone = c_tone->load() / 6.0f;
    s.colorRefl = c_colorRefl->load() > 0.5f;

    s.duck.on = d_on->load() > 0.5f;
    s.duck.hpHz = d_hp->load();
    s.duck.lpHz = d_lp->load();
    s.duck.mode = (DetMode) juce::roundToInt (d_det->load());
    s.duck.rmsMs = d_rms->load();
    s.duck.threshDb = d_thresh->load();
    s.duck.depthDb = d_depth->load();
    s.duck.full = d_full->load() > 0.5f;
    s.duck.kneeDb = d_knee->load();
    s.duck.attackMs = d_attack->load();
    s.duck.holdMs = d_hold->load();
    s.duck.releaseMs = d_release->load();
    s.duckSource = (DuckSource) juce::roundToInt (d_source->load());
    s.duckTarget = (DuckTarget) juce::roundToInt (d_target->load());
    s.duckPos = (DuckPos) juce::roundToInt (d_pos->load());
    s.duckRelSync = d_relSync->load() > 0.5f;
    s.duckRelNote = juce::roundToInt (d_relNote->load());
    s.lookaheadMs = d_look->load();
    return s;
}

void ProgDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int numIn = mainIn.getNumChannels();

    // Clear output channels that carry no input.
    for (int ch = numIn; ch < mainOut.getNumChannels(); ++ch)
        if (ch != 1 || numIn != 1)
            mainOut.clear (ch, 0, mainOut.getNumSamples());

    std::optional<int> midiPattern;
#if PD_MIDI_SWITCHING
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isProgramChange() && msg.getProgramChangeNumber() < kNumPatterns)
            midiPattern = msg.getProgramChangeNumber();
        else if (msg.isNoteOn() && msg.getNoteNumber() >= 36 && msg.getNoteNumber() < 36 + kNumPatterns)
            midiPattern = msg.getNoteNumber() - 36;  // C1 .. G1
    }
#endif
    juce::ignoreUnused (midi);

    const juce::AudioBuffer<float>* sc = nullptr;
    juce::AudioBuffer<float> scBuf;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        scBuf = getBusBuffer (buffer, true, 1);  // a view, no allocation
        if (scBuf.getNumChannels() > 0)
            sc = &scBuf;
    }
    engine.process (mainOut, numIn, HostTiming::fromPlayHead (getPlayHead()), readSettings(), midiPattern, sc);
}

double ProgDelayProcessor::getTailLengthSeconds() const
{
    const double bpm = std::max (30.0, (double) engine.bpmDisplay.load());
    double tail = 4.0 * secondsPerWhole (bpm);  // longest stage-1 grid
    const double fb = pFeedback->load() * 0.01;
    if (fb > 0.0)
    {
        // repeats until -60 dB (the soft clipper keeps 100 % bounded, not decaying: cap)
        const double repeats = fb >= 0.999 ? 1.0e9 : std::ceil (std::log (0.001) / std::log (fb));
        const double bar = engine.getFeedbackBarSamples() / std::max (1.0, engine.getSampleRate());
        tail += std::min (120.0, repeats * bar);
    }
    return std::min (tail, 150.0);
}

juce::AudioProcessorEditor* ProgDelayProcessor::createEditor()
{
    return new ProgDelayEditor (*this);
}

juce::ValueTree ProgDelayProcessor::captureState()
{
    auto st = StateSerializer::makeState (apvts, bank);
    st.setProperty ("presetName", currentPresetName, nullptr);
    return st;
}

void ProgDelayProcessor::restoreState (const juce::ValueTree& state)
{
    if (StateSerializer::applyState (state, apvts, bank) && state.hasProperty ("presetName"))
        currentPresetName = state.getProperty ("presetName").toString();
    publishPatterns();
}

void ProgDelayProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    StateSerializer::writeBinary (captureState(), dest);
}

void ProgDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    restoreState (StateSerializer::readBinary (data, sizeInBytes));
}
} // namespace progdelay
