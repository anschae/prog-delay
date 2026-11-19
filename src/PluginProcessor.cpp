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

    bank.getTree().addListener (this);
    publishPatterns();
}

ProgDelayProcessor::~ProgDelayProcessor()
{
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
    engine.prepare (sampleRate, samplesPerBlock);
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

    engine.process (mainOut, numIn, HostTiming::fromPlayHead (getPlayHead()), readSettings(), midiPattern);
}

double ProgDelayProcessor::getTailLengthSeconds() const
{
    const double bpm = std::max (30.0, (double) engine.bpmDisplay.load());
    return 4.0 * secondsPerWhole (bpm);  // longest stage-1 grid
}

juce::AudioProcessorEditor* ProgDelayProcessor::createEditor()
{
    return new ProgDelayEditor (*this);
}

juce::ValueTree ProgDelayProcessor::captureState()
{
    return StateSerializer::makeState (apvts, bank);
}

void ProgDelayProcessor::restoreState (const juce::ValueTree& state)
{
    StateSerializer::applyState (state, apvts, bank);
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
