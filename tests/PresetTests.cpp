#include "TestHelpers.h"
#include "PluginProcessor.h"
#include "model/PresetManager.h"
#include "model/FactoryPresets.h"

using namespace progdelay;

TEST_CASE ("User presets: save, list, load, delete", "[presets]")
{
    auto dir = juce::File::createTempFile ("pd-presets");
    dir.createDirectory();
    PresetManager::setUserFolderOverride (dir);

    ProgDelayProcessor p;
    auto& pm = p.getPresets();
    p.getBank().setName (2, "Saved Name");
    p.getApvts().getParameter (pid::feedback)->setValueNotifyingHost (0.77f);
    REQUIRE (pm.saveUser ("My Preset"));
    CHECK (dir.getChildFile ("My Preset.pdpreset").existsAsFile());
    CHECK (pm.getCurrentDisplayName() == "User / My Preset");

    // change things, then load it back
    p.getBank().setName (2, "Other");
    p.getApvts().getParameter (pid::feedback)->setValueNotifyingHost (0.1f);
    const auto all = pm.list();
    const auto it = std::find_if (all.begin(), all.end(), [] (auto& e) { return ! e.factory && e.name == "My Preset"; });
    REQUIRE (it != all.end());
    REQUIRE (pm.load (*it));
    CHECK (p.getBank().getPattern (2).name == "Saved Name");
    CHECK (p.getApvts().getParameter (pid::feedback)->getValue() == Catch::Approx (0.77f).margin (0.002));

    // factory presets come first; next / prev cycle
    CHECK (all.front().factory);
    pm.load (all.front());
    pm.loadNext (+1);
    CHECK (pm.getCurrentDisplayName() == all[1].displayName());
    pm.loadNext (-1);
    pm.loadNext (-1);
    CHECK (pm.getCurrentDisplayName() == all.back().displayName());

    CHECK (pm.deleteUser (*it));
    CHECK_FALSE (dir.getChildFile ("My Preset.pdpreset").existsAsFile());
    dir.deleteRecursively();
    PresetManager::setUserFolderOverride ({});
}

TEST_CASE ("A/B compare keeps two independent settings", "[presets]")
{
    ProgDelayProcessor p;
    auto& pm = p.getPresets();
    auto* fb = p.getApvts().getParameter (pid::feedback);
    fb->setValueNotifyingHost (0.2f);
    pm.switchAB (1);              // B starts as a copy of A
    CHECK (fb->getValue() == Catch::Approx (0.2f).margin (0.002));
    fb->setValueNotifyingHost (0.9f);
    p.getBank().setName (0, "B only");
    pm.switchAB (0);
    CHECK (fb->getValue() == Catch::Approx (0.2f).margin (0.002));
    CHECK (p.getBank().getPattern (0).name != "B only");
    pm.switchAB (1);
    CHECK (fb->getValue() == Catch::Approx (0.9f).margin (0.002));
    CHECK (p.getBank().getPattern (0).name == "B only");
}

TEST_CASE ("The preset name survives a project reload", "[presets]")
{
    ProgDelayProcessor a;
    a.getPresets().load ({ FactoryPresets::all()[2].name, true, 2, {} });
    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    ProgDelayProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (b.getCurrentPresetName() == "Factory / " + FactoryPresets::all()[2].name);
}
