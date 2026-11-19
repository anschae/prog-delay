#include "PresetBar.h"
#include "PluginProcessor.h"

namespace progdelay
{
PresetBar::PresetBar (ProgDelayProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &name, &next })
        addAndMakeVisible (b);
    addAndMakeVisible (ab);
    prev.setConnectedEdges (juce::Button::ConnectedOnRight);
    name.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    next.setConnectedEdges (juce::Button::ConnectedOnLeft);
    prev.onClick = [this] { proc.getPresets().loadNext (-1); refresh(); };
    next.onClick = [this] { proc.getPresets().loadNext (+1); refresh(); };
    name.onClick = [this] { showMenu(); };
    name.setColour (juce::TextButton::textColourOffId, theme::text);
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    name.setTooltip ("Presets: load, save, delete");
    ab.setTooltip ("A/B compare: switch between two complete settings. Right-click the menu for 'Copy to other'.");
    ab.onChange = [this] (int i) { proc.getPresets().switchAB (i); refresh(); };
    refresh();
    startTimerHz (4);
}

PresetBar::~PresetBar() = default;

void PresetBar::refresh()
{
    name.setButtonText (proc.getPresets().getCurrentDisplayName());
    ab.setSelected (proc.getPresets().getActiveAB());
}

void PresetBar::timerCallback() { refresh(); }

void PresetBar::resized()
{
    auto r = getLocalBounds();
    ab.setBounds (r.removeFromRight (64));
    r.removeFromRight (10);
    prev.setBounds (r.removeFromLeft (30));
    next.setBounds (r.removeFromRight (30));
    name.setBounds (r);
}

void PresetBar::showMenu()
{
    auto& pm = proc.getPresets();
    const auto all = pm.list();
    juce::PopupMenu factory, user, m;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& e = all[(size_t) i];
        const bool ticked = e.displayName() == pm.getCurrentDisplayName();
        (e.factory ? factory : user).addItem (1 + i, e.name, true, ticked);
    }
    m.addSubMenu ("Factory", factory);
    m.addSubMenu ("User", user, user.getNumItems() > 0);
    m.addSeparator();
    m.addItem (10001, "Save preset...");
    const auto current = pm.getCurrentDisplayName();
    const bool isUser = current.startsWith ("User / ");
    m.addItem (10002, "Delete '" + pm.getCurrentName() + "'", isUser);
    m.addSeparator();
    m.addItem (10003, juce::String ("Copy ") + (pm.getActiveAB() == 0 ? "A to B" : "B to A"));
    m.addItem (10004, "Show preset folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name), [this, all] (int r) {
        auto& presets = proc.getPresets();
        if (r >= 1 && r <= (int) all.size())
            presets.load (all[(size_t) r - 1]);
        else if (r == 10001)
            askSaveName();
        else if (r == 10002)
        {
            for (const auto& e : all)
                if (e.displayName() == presets.getCurrentDisplayName())
                    presets.deleteUser (e);
        }
        else if (r == 10003)
            presets.copyActiveToOther();
        else if (r == 10004)
        {
            PresetManager::getUserFolder().createDirectory();
            PresetManager::getUserFolder().revealToUser();
        }
        refresh();
    });
}

void PresetBar::askSaveName()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name of the preset:", juce::MessageBoxIconType::NoIcon, this);
    const auto cur = proc.getPresets().getCurrentName();
    w->addTextEditor ("name", cur == "Init" ? juce::String() : cur);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result) {
        if (result == 1)
            proc.getPresets().saveUser (w->getTextEditorContents ("name"));
        refresh();
    }), true);
}
} // namespace progdelay
