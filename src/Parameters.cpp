#include "Parameters.h"
#include "dsp/modes/SoundModeRegistry.h"

namespace progdelay
{
namespace
{
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using API = juce::AudioParameterInt;
using Attr = juce::AudioParameterFloatAttributes;

juce::ParameterID id (const char* s) { return { s, kParamVersion }; }

juce::String fmt (float v, int decimals, const char* unit)
{
    return juce::String (v, decimals) + unit;
}

std::unique_ptr<APF> percent (const char* pidStr, const juce::String& name, float lo, float hi, float def)
{
    return std::make_unique<APF> (id (pidStr), name, juce::NormalisableRange<float> (lo, hi, 0.1f), def,
                                  Attr().withLabel ("%").withStringFromValueFunction (
                                      [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));
}

std::unique_ptr<APF> decibels (const char* pidStr, const juce::String& name, float lo, float hi, float def, bool floorIsInf)
{
    return std::make_unique<APF> (id (pidStr), name, juce::NormalisableRange<float> (lo, hi, 0.1f), def,
                                  Attr().withLabel ("dB").withStringFromValueFunction ([lo, floorIsInf] (float v, int) {
                                      if (floorIsInf && v <= lo + 0.01f)
                                          return juce::String ("-inf dB");
                                      return juce::String (v, 1) + " dB";
                                  }));
}

std::unique_ptr<APF> hertz (const char* pidStr, const juce::String& name, float lo, float hi, float def)
{
    juce::NormalisableRange<float> r (lo, hi, 1.0f);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return std::make_unique<APF> (id (pidStr), name, r, def,
                                  Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) {
                                      return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                                          : juce::String (juce::roundToInt (v)) + " Hz";
                                  }));
}

std::unique_ptr<APF> millis (const char* pidStr, const juce::String& name, float lo, float hi, float def, float centre)
{
    juce::NormalisableRange<float> r (lo, hi, 0.01f);
    r.setSkewForCentre (centre);
    return std::make_unique<APF> (id (pidStr), name, r, def,
                                  Attr().withLabel ("ms").withStringFromValueFunction ([] (float v, int) {
                                      if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
                                      if (v < 10.0f) return juce::String (v, 1) + " ms";
                                      return juce::String (juce::roundToInt (v)) + " ms";
                                  }));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- Output
    layout.add (percent (pid::mix, "Mix", 0.0f, 100.0f, 35.0f));
    layout.add (decibels (pid::dry, "Dry Level", -60.0f, 6.0f, 0.0f, true));
    layout.add (decibels (pid::wet, "Wet Level", -60.0f, 6.0f, 0.0f, true));
    layout.add (std::make_unique<APB> (id (pid::killDry), "Kill Dry", false));
    layout.add (percent (pid::width, "Width", 0.0f, 200.0f, 100.0f));

    // ---- Stage 2 (feedback)
    layout.add (percent (pid::feedback, "Feedback", 0.0f, 100.0f, 30.0f));
    layout.add (std::make_unique<APC> (id (pid::fbBarMode), "Feedback Bar", juce::StringArray { "Host", "Manual" }, 0));
    layout.add (std::make_unique<API> (id (pid::fbBarNum), "Feedback Bar Numerator", 1, 32, 4));
    layout.add (std::make_unique<APC> (id (pid::fbBarDen), "Feedback Bar Denominator", juce::StringArray { "2", "4", "8", "16" }, 1));
    layout.add (decibels (pid::s1Level, "Stage 1 Level", -60.0f, 6.0f, 0.0f, true));
    layout.add (decibels (pid::s2Level, "Stage 2 Level", -60.0f, 6.0f, 0.0f, true));
    layout.add (hertz (pid::lowCut, "Low Cut", 20.0f, 2000.0f, 20.0f));
    layout.add (hertz (pid::highCut, "High Cut", 1000.0f, 20000.0f, 20000.0f));
    layout.add (std::make_unique<APC> (id (pid::stereoMode), "Stereo Mode", juce::StringArray { "Keep", "Ping-Pong", "Mono" }, 0));

    // ---- Sound mode
    layout.add (std::make_unique<APC> (id (pid::soundMode), "Sound Mode", SoundModeRegistry::get().getDisplayNames(), 0));
    layout.add (percent (pid::character, "Character", 0.0f, 100.0f, 50.0f));
    layout.add (percent (pid::age, "Age/Mod", 0.0f, 100.0f, 30.0f));
    layout.add (std::make_unique<APF> (id (pid::tone), "Tone", juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f,
                                       Attr().withLabel ("dB").withStringFromValueFunction ([] (float v, int) {
                                           return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
                                       })));
    layout.add (std::make_unique<APB> (id (pid::colorRefl), "Color Reflections", false));

    // ---- Ducking
    layout.add (std::make_unique<APB> (id (pid::duckOn), "Ducking On", false));
    layout.add (std::make_unique<APC> (id (pid::duckSource), "Duck Source", juce::StringArray { "Input", "Sidechain" }, 0));
    layout.add (hertz (pid::duckHP, "Duck Detector HP", 20.0f, 1000.0f, 100.0f));
    layout.add (hertz (pid::duckLP, "Duck Detector LP", 1000.0f, 20000.0f, 20000.0f));
    layout.add (std::make_unique<APC> (id (pid::duckDetMode), "Duck Detector", juce::StringArray { "Peak", "RMS" }, 0));
    layout.add (millis (pid::duckRms, "Duck RMS Window", 10.0f, 100.0f, 30.0f, 40.0f));
    layout.add (decibels (pid::duckThresh, "Duck Threshold", -60.0f, 0.0f, -24.0f, false));
    layout.add (decibels (pid::duckDepth, "Duck Depth", -48.0f, 0.0f, -12.0f, false));
    layout.add (std::make_unique<APB> (id (pid::duckFull), "Duck Full", false));
    layout.add (std::make_unique<APF> (id (pid::duckKnee), "Duck Knee", juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 6.0f,
                                       Attr().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return fmt (v, 1, " dB"); })));
    layout.add (millis (pid::duckAttack, "Duck Attack", 0.1f, 100.0f, 5.0f, 10.0f));
    layout.add (millis (pid::duckHold, "Duck Hold", 0.0f, 500.0f, 50.0f, 100.0f));
    layout.add (millis (pid::duckRelease, "Duck Release", 10.0f, 3000.0f, 250.0f, 300.0f));
    layout.add (std::make_unique<APB> (id (pid::duckRelSync), "Duck Release Sync", false));
    layout.add (std::make_unique<APC> (id (pid::duckRelNote), "Duck Release Note",
                                       juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 bar" }, 1));
    layout.add (millis (pid::duckLook, "Duck Lookahead", 0.0f, 10.0f, 0.0f, 3.0f));
    layout.add (std::make_unique<APC> (id (pid::duckTarget), "Duck Target", juce::StringArray { "Stage 1", "Stage 2", "Both" }, 2));
    layout.add (std::make_unique<APC> (id (pid::duckPos), "Duck Position", juce::StringArray { "Pre", "Post" }, 1));

    // ---- Tempo and patterns
    layout.add (std::make_unique<APC> (id (pid::tempoMode), "Tempo Mode", juce::StringArray { "Sync", "Manual" }, 0));
    layout.add (std::make_unique<APF> (id (pid::manualBpm), "Manual BPM", juce::NormalisableRange<float> (30.0f, 300.0f, 0.01f), 120.0f,
                                       Attr().withLabel ("BPM").withStringFromValueFunction ([] (float v, int) { return fmt (v, 2, " BPM"); })));
    layout.add (std::make_unique<API> (id (pid::activePattern), "Active Pattern", 1, 8, 1));
    layout.add (std::make_unique<APC> (id (pid::switchSource), "Switch Source",
                                       juce::StringArray { "Automation", "Follow sig.", "MIDI" }, 0));
    layout.add (std::make_unique<APC> (id (pid::switchQuant), "Switch Quantize",
                                       juce::StringArray { "Immediate", "Next beat", "Next bar" }, 0));
    layout.add (std::make_unique<APC> (id (pid::fallback), "Fallback Pattern",
                                       juce::StringArray { "Keep current", "1", "2", "3", "4", "5", "6", "7", "8" }, 0));
    return layout;
}
} // namespace progdelay
