// Developer tool: renders the plugin editor to PNG files, without a DAW.
//
//   ProgDelaySnapshot <output-folder> [factory-preset-name]
//
// Build with -DPD_BUILD_TOOLS=ON. On Linux without a display, run it under xvfb-run.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "model/FactoryPresets.h"

namespace
{
void pump (int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

bool save (juce::Component& c, const juce::File& f, float scale = 1.0f)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    f.deleteFile();
    juce::FileOutputStream s (f);
    juce::PNGImageFormat png;
    const bool ok = s.openedOk() && png.writeImageToStream (img, s);
    std::cout << (ok ? "wrote " : "FAILED ") << f.getFullPathName() << std::endl;
    return ok;
}

struct FakePlayHead : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (132.0);
        p.setTimeSignature (TimeSignature { 7, 8 });
        p.setIsPlaying (true);
        p.setPpqPosition (ppq);
        p.setPpqPositionOfLastBarStart (0.0);
        return p;
    }
    double ppq = 1.4;
};
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 2)
    {
        std::cout << "usage: ProgDelaySnapshot <output-folder> [factory-preset]" << std::endl;
        return 1;
    }
    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    out.createDirectory();

    progdelay::ProgDelayProcessor proc;
    const juce::String presetName = argc > 2 ? juce::String (argv[2]) : juce::String ("7/8 Djent Stab");
    for (const auto& e : proc.getPresets().list())
        if (e.name == presetName)
            proc.getPresets().load (e);

    FakePlayHead ph;
    proc.setPlayHead (&ph);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (4, 512);
    juce::MidiBuffer midi;
    juce::Random rng (1);
    for (int b = 0; b < 200; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, (b % 40 < 6) ? (rng.nextFloat() - 0.5f) * 0.8f : 0.0f);
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    if (auto* pe = dynamic_cast<progdelay::ProgDelayEditor*> (ed.get()))
    {
        pe->getView().model.selectedTick = progdelay::straightTick (3, 8);
        pe->getView().model.notify();
    }
    pump (200);
    for (int b = 0; b < 400; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, (b % 60 < 12) ? (rng.nextFloat() - 0.5f) * (0.2f + 0.02f * (float) (b % 60)) : 0.0f);
        proc.processBlock (buf, midi);
        if (b % 8 == 0)
            pump (15);
    }
    pump (200);
    save (*ed, out.getChildFile ("editor.png"));
    save (*ed, out.getChildFile ("editor@2x.png"), 2.0f);

    ed.reset();
    proc.setPlayHead (nullptr);
    return 0;
}
