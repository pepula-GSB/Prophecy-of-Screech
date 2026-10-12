#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace Palette
{
    const juce::Colour bg        { 0xff060406 };
    const juce::Colour panel     { 0xff0e0a0c };
    const juce::Colour panelEdge { 0xff3a1219 };
    const juce::Colour red       { 0xffff1e3c };
    const juce::Colour deepRed   { 0xff8a0018 };
    const juce::Colour ember     { 0xffff6a2a };
    const juce::Colour text      { 0xffefe4e4 };
    const juce::Colour dimText   { 0xff8d7a7d };
}

// ------------------------------------------------------------------ look & feel
class ProphecyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ProphecyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// ------------------------------------------------------------------ knob
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& label,
          bool bipolar = false, bool big = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String label;
    bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ------------------------------------------------------------------ toggle
class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& label);
    void resized() override;
private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// ------------------------------------------------------------------ oscilloscope
class Scope : public juce::Component, private juce::Timer
{
public:
    explicit Scope (ProphecyProcessor&);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    ProphecyProcessor& proc;
};

// ------------------------------------------------------------------ main canvas (fixed 1280x760, scaled by editor)
class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1280, H = 760;
    explicit Canvas (ProphecyProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawSigil (juce::Graphics&);

    struct Section { juce::String title; juce::Rectangle<int> area; };
    struct RowItem { juce::String title; std::vector<juce::Component*> cells; juce::Component* wide = nullptr; float wideWeight = 0.f; };
    void layoutRow (int y, int h, const std::vector<RowItem>& items);

    ProphecyProcessor& proc;
    ProphecyLookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Section> sections;
    std::vector<RowItem> row0, row1, row2;

    Knob* add (const char* id, const juce::String& label, bool bipolar = false, bool big = false);
    Toggle* addToggle (const char* id, const juce::String& label);

    Knob* rateKnob = nullptr;
    Knob* divKnob = nullptr;
    Scope scope;
    juce::ComboBox presetBox;
    float angle = 0.f;
    int lastProgram = -1;
};

// ------------------------------------------------------------------ editor
class ProphecyEditor : public juce::AudioProcessorEditor
{
public:
    explicit ProphecyEditor (ProphecyProcessor&);
    void resized() override;
private:
    Canvas canvas;
};
