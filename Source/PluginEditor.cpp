#include "PluginEditor.h"

using namespace juce;

static Font uiFont (float size, bool bold = true, float kerning = 0.f)
{
    Font f (FontOptions (size).withStyle (bold ? "Bold" : "Regular"));
    return kerning > 0.f ? f.withExtraKerningFactor (kerning) : f;
}

// ====================================================================== LookAndFeel
ProphecyLookAndFeel::ProphecyLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff120c0e));
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::deepRed);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::arrowColourId, Palette::red);
    setColour (Label::textColourId, Palette::text);
    setColour (TextEditor::backgroundColourId, Colour (0xff1a1012));
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::highlightColourId, Palette::deepRed);
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, Palette::red);
    setColour (CaretComponent::caretColourId, Palette::red);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxTextColourId, Palette::dimText);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (AlertWindow::backgroundColourId, Colour (0xff120c0e));
    setColour (AlertWindow::textColourId, Palette::text);
    setColour (AlertWindow::outlineColourId, Palette::red);
    setColour (TextButton::textColourOffId, Palette::text);
    setColour (TextButton::textColourOnId, Colours::white);
}

void ProphecyLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (highlighted)
    {
        g.setColour (Palette::red.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (2.f), 6.f);
    }
    ColourGradient bgGrad (down ? Palette::deepRed : Colour (0xff1c1114), r.getX(), r.getY(),
                           down ? Palette::deepRed.darker (0.4f) : Colour (0xff0e090a), r.getX(), r.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (highlighted ? Palette::red : Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (r, 4.f, 1.2f);
}

void ProphecyLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool highlighted, bool)
{
    g.setFont (uiFont (12.f, true, 0.18f));
    g.setColour (highlighted ? Colours::white : Palette::text);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

Font ProphecyLookAndFeel::getAlertWindowTitleFont()   { return uiFont (16.f, true, 0.2f); }
Font ProphecyLookAndFeel::getAlertWindowMessageFont() { return uiFont (13.f, false); }

void ProphecyLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, Slider& s)
{
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const bool big     = (bool) s.getProperties()["big"];
    const bool enabled = s.isEnabled();

    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 10.f : 5.f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);
    const auto c = bounds.getCentre();
    const float r = size * 0.5f;
    const float arcR = r - (big ? 6.f : 3.f);
    const float thick = big ? 6.f : 3.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // tick marks
    const int ticks = big ? 31 : 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
        const float lit = (a <= angle + 0.001f) ? 1.f : 0.f;
        g.setColour ((lit > 0.f ? Palette::red : Palette::panelEdge).withAlpha (enabled ? 0.9f : 0.4f));
        const Point<float> p1 = c.getPointOnCircumference (r + 1.f, a);
        g.fillEllipse (Rectangle<float> (big ? 2.6f : 2.f, big ? 2.6f : 2.f).withCentre (p1));
    }

    // background track
    Path track;
    track.addCentredArc (c.x, c.y, arcR - 4.f, arcR - 4.f, 0.f, startAngle, endAngle, true);
    g.setColour (Colour (0xff22161a));
    g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc (+glow)
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, arcR - 4.f, arcR - 4.f, 0.f, jmin (from, angle), jmax (from, angle), true);
        const float alpha = enabled ? 1.f : 0.35f;
        g.setColour (Palette::red.withAlpha (0.18f * alpha));
        g.strokePath (arc, PathStrokeType (thick * 3.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::red.withAlpha (0.35f * alpha));
        g.strokePath (arc, PathStrokeType (thick * 1.9f, PathStrokeType::curved, PathStrokeType::rounded));
        ColourGradient grad (Palette::deepRed.withAlpha (alpha), c.x - arcR, c.y + arcR,
                             Palette::ember.withAlpha (alpha), c.x + arcR, c.y - arcR, false);
        grad.addColour (0.5, Palette::red.withAlpha (alpha));
        g.setGradientFill (grad);
        g.strokePath (arc, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // body
    const float bodyR = arcR - thick - (big ? 8.f : 4.f);
    auto body = Rectangle<float> (bodyR * 2.f, bodyR * 2.f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillEllipse (body.translated (0.f, 2.f).expanded (1.f));
    ColourGradient bodyGrad (Colour (0xff2e2427), c.x, body.getY(),
                             Colour (0xff0a0708), c.x, body.getBottom(), false);
    g.setGradientFill (bodyGrad);
    g.fillEllipse (body);
    g.setColour (Colour (0xff3d2a2f));
    g.drawEllipse (body.reduced (0.5f), 1.f);

    if (big)
    {
        // inner sigil ring on the macro knob
        g.setColour (Palette::red.withAlpha (0.25f));
        g.drawEllipse (body.reduced (bodyR * 0.28f), 1.f);
        Path tri;
        const float tr = bodyR * 0.55f;
        for (int i = 0; i < 3; ++i)
        {
            const auto p = c.getPointOnCircumference (tr, angle + MathConstants<float>::pi + (float) i * MathConstants<float>::twoPi / 3.f);
            if (i == 0) tri.startNewSubPath (p); else tri.lineTo (p);
        }
        tri.closeSubPath();
        g.setColour (Palette::red.withAlpha (0.18f + 0.5f * pos));
        g.strokePath (tri, PathStrokeType (1.2f));
    }

    // pointer
    const auto p1 = c.getPointOnCircumference (bodyR * 0.25f, angle);
    const auto p2 = c.getPointOnCircumference (bodyR * 0.88f, angle);
    g.setColour (Palette::red.withAlpha (0.4f));
    g.drawLine ({ p1, p2 }, big ? 6.f : 4.f);
    g.setColour (enabled ? Palette::text : Palette::dimText);
    g.drawLine ({ p1, p2 }, big ? 2.6f : 2.f);
}

void ProphecyLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const bool on = b.getToggleState();
    g.setColour (on ? Palette::deepRed.withAlpha (0.55f) : Colour (0xff160f11));
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (on ? Palette::red : (highlighted ? Palette::dimText : Palette::panelEdge));
    g.drawRoundedRectangle (r, 5.f, 1.2f);

    // LED
    auto led = Rectangle<float> (7.f, 7.f).withCentre ({ r.getX() + 11.f, r.getCentreY() });
    if (on)
    {
        g.setColour (Palette::red.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.f));
    }
    g.setColour (on ? Palette::red : Colour (0xff3a2a2e));
    g.fillEllipse (led);

    g.setColour (on ? Palette::text : Palette::dimText);
    g.setFont (uiFont (10.5f, true, 0.04f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (18.f), Justification::centred);
}

void ProphecyLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colour (0xff130c0e));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (box.hasKeyboardFocus (true) || box.isMouseOver (true) ? Palette::red : Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (r, 4.f, 1.2f);

    Path arrow;
    const float ax = (float) w - 16.f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5.f, ay - 3.f, ax + 5.f, ay - 3.f, ax, ay + 4.f);
    g.setColour (Palette::red);
    g.fillPath (arrow);
}

Font ProphecyLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (14.f, true, 0.08f); }
Font ProphecyLookAndFeel::getPopupMenuFont()          { return uiFont (14.f, false); }

Label* ProphecyLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont ((bool) s.getProperties()["big"] ? 14.f : 11.f, false));
    l->setColour (Label::textColourId, Palette::dimText);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setJustificationType (Justification::centred);
    return l;
}

// ====================================================================== Knob
Knob::Knob (AudioProcessorValueTreeState& apvts, const char* paramId, const String& text, bool bipolar, bool isBig)
    : label (text), big (isBig)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("big", isBig);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, isBig ? 20 : 16);
    slider.setMouseDragSensitivity (isBig ? 320 : 220);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, Palette::dimText);
    slider.setColour (Slider::textBoxHighlightColourId, Palette::deepRed);
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

    if (auto* p = apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.setTooltip (apvts.getParameter (paramId) != nullptr ? apvts.getParameter (paramId)->getName (64) : String());
}

void Knob::paint (Graphics& g)
{
    g.setColour (isEnabled() ? Palette::text : Palette::dimText);
    g.setFont (uiFont (big ? 20.f : 11.5f, true, big ? 0.35f : 0.14f));
    g.drawText (label, getLocalBounds().removeFromTop (big ? 28 : 18), Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (big ? 28 : 18);
    slider.setBounds (r);
}

// ====================================================================== Toggle
Toggle::Toggle (AudioProcessorValueTreeState& apvts, const char* paramId, const String& text)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, button);
}

void Toggle::resized()
{
    button.setBounds (getLocalBounds().withSizeKeepingCentre (jmin (getWidth() - 2, 104), 30));
}

// ====================================================================== Scope
Scope::Scope (ProphecyProcessor& p) : proc (p) { startTimerHz (30); }

void Scope::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff080506));
    g.fillRoundedRectangle (r, 6.f);

    // grid
    g.setColour (Palette::panelEdge.withAlpha (0.6f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 8.f), r.getY() + 4.f, r.getBottom() - 4.f);
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.f), r.getX() + 4.f, r.getRight() - 4.f);

    // grab samples, align to rising zero crossing
    constexpr int N = ProphecyProcessor::scopeSize;
    constexpr int show = 520;
    std::array<float, N> local;
    const int w = proc.scopeWritePos.load (std::memory_order_acquire);
    for (int i = 0; i < N; ++i)
        local[(size_t) i] = proc.scopeBuffer[(size_t) ((w + i) & (N - 1))];

    int start = N - show - 1;
    for (int i = N - show - 1; i > N - show - 1 - 1500 && i > 1; --i)
        if (local[(size_t) i - 1] < 0.f && local[(size_t) i] >= 0.f) { start = i; break; }

    Path wave;
    const float midY = r.getCentreY();
    const float amp = r.getHeight() * 0.44f;
    for (int i = 0; i < show; ++i)
    {
        const float xx = r.getX() + 4.f + (r.getWidth() - 8.f) * (float) i / (float) (show - 1);
        const float yy = midY - jlimit (-1.f, 1.f, local[(size_t) (start + i)]) * amp;
        if (i == 0) wave.startNewSubPath (xx, yy); else wave.lineTo (xx, yy);
    }

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (Palette::red.withAlpha (0.15f));
    g.strokePath (wave, PathStrokeType (6.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Palette::red.withAlpha (0.35f));
    g.strokePath (wave, PathStrokeType (3.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Palette::ember.brighter (0.4f));
    g.strokePath (wave, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
    g.restoreState();

    g.setColour (Palette::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
}

// ====================================================================== Canvas
Knob* Canvas::add (const char* id, const String& label, bool bipolar, bool big)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, bipolar, big);
    auto* raw = k.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (k));
    return raw;
}

Toggle* Canvas::addToggle (const char* id, const String& label)
{
    auto t = std::make_unique<Toggle> (proc.apvts, id, label);
    auto* raw = t.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (t));
    return raw;
}

Canvas::Canvas (ProphecyProcessor& p) : proc (p), scope (p)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;

    row0 = {
        { "OSCILLATOR", { add (wave, "WAVE"), add (unison, "UNISON"), add (detune, "DETUNE"),
                          add (width, "WIDTH"), add (fm, "FM"), add (fmRatio, "RATIO") } },
        { "PITCH", { add (glide, "GLIDE"), add (penv, "P.ENV", true), add (pdecay, "P.DECAY"),
                     add (bend, "BEND"), addToggle (legato, "LEGATO") } },
        { "DISTORTION", { add (dtype, "TYPE"), add (drive, "DRIVE"), add (bite, "BITE"), add (dmix, "MIX") } },
    };
    row1 = {
        { "FILTER", { add (ftype, "TYPE"), add (cutoff, "CUTOFF"), add (reso, "RESO"),
                      add (fenv, "ENV", true), add (fdecay, "DECAY"), add (keytrack, "KEY") } },
        { "LFO", { add (lshape, "SHAPE"), addToggle (lsync, "SYNC"), (rateKnob = add (lrate, "RATE")),
                   (divKnob = add (lfoDiv, "DIV")), add (lcut, "> CUTOFF", true), add (lpitch, "> PITCH", true) } },
        { "THE SCREAM", { add (scream, "SCREAM", false, true) }, nullptr, 1.4f },
    };
    row2 = {
        { "AMP", { add (attack, "ATTACK"), add (decay, "DECAY"), add (sustain, "SUSTAIN"), add (release, "RELEASE") } },
        { "DELAY", { add (dtime, "TIME"), add (dfb, "FDBK"), add (dfx, "MIX"), addToggle (pingpong, "P.PONG") } },
        { "REVERB", { add (rsize, "SIZE"), add (rdamp, "DAMP"), add (rmix, "MIX") } },
        { "MASTER", { add (volume, "VOLUME") }, &scope, 3.2f },
    };
    addAndMakeVisible (scope);

    refreshPresetList();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userIdBase)
            proc.setCurrentProgram (id - 1);
        else if (isPositiveAndBelow (id - userIdBase, userPresetFiles.size()))
            proc.loadUserPreset (userPresetFiles[id - userIdBase]);
        lastPresetName = proc.getPresetName();
    };
    presetBox.setTooltip ("Presety fabryczne i Twoje zapisane presety");

    saveButton.onClick = [this] { showSaveDialog(); };
    saveButton.setTooltip ("Zapisz obecne brzmienie jako preset");
    randomButton.onClick = [this] { proc.randomize(); };
    randomButton.setTooltip ("Losowy screech w rozsadnych zakresach");
    addAndMakeVisible (saveButton);
    addAndMakeVisible (randomButton);
    addAndMakeVisible (presetBox);

    setSize (W, H);
    timerCallback();   // show preset name / sync state right away
    startTimerHz (30);
}

Canvas::~Canvas()
{
    if (saveDialog != nullptr) saveDialog->exitModalState (0);
    saveDialog.reset();
    setLookAndFeel (nullptr);
}

void Canvas::refreshPresetList()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    userPresetFiles = proc.getUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    }
    lastPresetName = {};   // force the timer to re-sync the displayed name
}

void Canvas::showSaveDialog()
{
    if (saveDialog != nullptr) return;

    saveDialog = std::make_unique<AlertWindow> ("SAVE PRESET",
                                                "Nazwa presetu (zapis do Dokumenty/Prophecy of Scream/Presets):",
                                                MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    auto suggested = proc.getPresetName().startsWith ("Random") ? String ("My Screech") : proc.getPresetName();
    saveDialog->addTextEditor ("name", suggested);
    saveDialog->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<Canvas> safe (this);
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveDialog == nullptr) return;
        const auto name = safe->saveDialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->saveDialog.reset(); });

        if (result == 1)
        {
            if (safe->proc.saveUserPreset (name))
                safe->refreshPresetList();
            else
                AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SAVE PRESET",
                                                  "Nie udalo sie zapisac presetu.");
        }
    }), false);
}

void Canvas::timerCallback()
{
    angle += 0.0035f;
    repaint (0, 0, W, 112);

    const bool sync = proc.apvts.getRawParameterValue (ParamIDs::lsync)->load() > 0.5f;
    if (rateKnob->isEnabled() == sync) { rateKnob->setEnabled (! sync); rateKnob->repaint(); }
    if (divKnob->isEnabled() != sync)  { divKnob->setEnabled (sync); divKnob->repaint(); }

    const auto name = proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == name) { found = presetBox.getItemId (i); break; }
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else            presetBox.setText (name, dontSendNotification);
    }
}

void Canvas::layoutRow (int y, int h, const std::vector<RowItem>& items)
{
    const int margin = 16, gap = 14;
    float total = 0.f;
    for (auto& it : items) total += (float) it.cells.size() + it.wideWeight;

    const float avail = (float) (W - 2 * margin - gap * ((int) items.size() - 1));
    float x = (float) margin;

    for (auto& it : items)
    {
        const float weight = (float) it.cells.size() + it.wideWeight;
        const int sw = (int) std::round (avail * weight / total);
        Rectangle<int> area ((int) x, y, sw, h);
        sections.push_back ({ it.title, area });

        auto inner = area.reduced (10, 8);
        inner.removeFromTop (24);

        if (it.wide != nullptr)
        {
            const int wideW = (int) ((float) inner.getWidth() * it.wideWeight / weight);
            it.wide->setBounds (inner.removeFromRight (wideW).reduced (6, 10));
        }

        const int n = (int) it.cells.size();
        const float cellW = (float) inner.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            auto cell = Rectangle<int> (inner.getX() + (int) (cellW * (float) i), inner.getY(), (int) cellW, inner.getHeight());
            const bool isBig = n == 1 && it.wide == nullptr;
            const int maxW = isBig ? 200 : 96;
            it.cells[(size_t) i]->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 4, maxW), cell.getHeight()));
        }
        x += (float) sw + (float) gap;
    }
}

void Canvas::resized()
{
    sections.clear();
    layoutRow (124, 200, row0);
    layoutRow (338, 200, row1);
    layoutRow (552, 192, row2);
    const int px = W - 16 - 230;
    presetBox.setBounds (px, 32, 230, 30);
    randomButton.setBounds (px, 70, 150, 28);
    saveButton.setBounds (px + 158, 70, 72, 28);
}

void Canvas::drawSigil (Graphics& g)
{
    const Point<float> c (W * 0.5f, 54.f);

    // laser fan
    for (int i = 0; i < 14; ++i)
    {
        const float a = -1.25f + 2.5f * (float) i / 13.f + std::sin (angle * 3.f + (float) i) * 0.03f;
        const auto end = c.getPointOnCircumference (900.f, a + MathConstants<float>::halfPi);
        ColourGradient lg (Palette::red.withAlpha (0.16f), c.x, c.y, Palette::red.withAlpha (0.f), end.x, end.y, false);
        g.setGradientFill (lg);
        g.drawLine ({ c, end }, 1.f);
        const auto end2 = c.getPointOnCircumference (900.f, -a - MathConstants<float>::halfPi);
        ColourGradient lg2 (Palette::red.withAlpha (0.16f), c.x, c.y, Palette::red.withAlpha (0.f), end2.x, end2.y, false);
        g.setGradientFill (lg2);
        g.drawLine ({ c, end2 }, 1.f);
    }

    // rings
    const float radii[] = { 44.f, 70.f, 118.f, 190.f, 300.f };
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (Palette::red.withAlpha (0.20f - 0.032f * (float) i));
        g.drawEllipse (Rectangle<float> (radii[i] * 2.f, radii[i] * 2.f).withCentre (c), i == 1 ? 1.4f : 1.f);
    }

    // rotating rune ticks
    for (int i = 0; i < 48; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 48.f;
        const float len = (i % 4 == 0) ? 12.f : 5.f;
        g.setColour (Palette::red.withAlpha (i % 4 == 0 ? 0.30f : 0.14f));
        g.drawLine ({ c.getPointOnCircumference (118.f, a), c.getPointOnCircumference (118.f + len, a) }, 1.f);
    }
    for (int i = 0; i < 24; ++i)
    {
        const float a = -angle * 1.6f + (float) i * MathConstants<float>::twoPi / 24.f;
        g.setColour (Palette::red.withAlpha (0.12f));
        g.drawLine ({ c.getPointOnCircumference (190.f, a), c.getPointOnCircumference (180.f, a) }, 1.f);
    }

    // counter-rotating triangles (hexagram)
    for (int t = 0; t < 2; ++t)
    {
        Path tri;
        const float base = (t == 0 ? angle * 2.f : -angle * 2.f + MathConstants<float>::pi);
        for (int i = 0; i < 3; ++i)
        {
            const auto p = c.getPointOnCircumference (70.f, base + (float) i * MathConstants<float>::twoPi / 3.f);
            if (i == 0) tri.startNewSubPath (p); else tri.lineTo (p);
        }
        tri.closeSubPath();
        g.setColour (Palette::red.withAlpha (0.22f));
        g.strokePath (tri, PathStrokeType (1.2f));
    }
}

void Canvas::paint (Graphics& g)
{
    // background
    g.fillAll (Palette::bg);
    ColourGradient glow (Colour (0xff4a0010).withAlpha (0.75f), W * 0.5f, 40.f,
                         Colours::transparentBlack, W * 0.5f, 520.f, true);
    g.setGradientFill (glow);
    g.fillRect (0, 0, W, H);
    ColourGradient floor (Colours::transparentBlack, 0.f, H * 0.6f, Colour (0xff1a0307), 0.f, (float) H, false);
    g.setGradientFill (floor);
    g.fillRect (0, H / 2, W, H / 2);

    // header
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (0, 0, W, 112);
        drawSigil (g);
    }

    const auto titleArea = Rectangle<int> (0, 14, W, 50);
    g.setFont (uiFont (40.f, true, 0.32f));
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (Palette::red.withAlpha (0.10f));
        g.drawText ("PROPHECY OF SCREAM", titleArea.translated (0, i), Justification::centred);
        g.drawText ("PROPHECY OF SCREAM", titleArea.translated (i, 0), Justification::centred);
        g.drawText ("PROPHECY OF SCREAM", titleArea.translated (-i, 0), Justification::centred);
    }
    g.setColour (Palette::text);
    g.drawText ("PROPHECY OF SCREAM", titleArea, Justification::centred);

    g.setFont (uiFont (11.5f, true, 0.45f));
    g.setColour (Palette::red);
    g.drawText ("HARDSTYLE  SCREECH  SYNTHESIZER", Rectangle<int> (0, 66, W, 18), Justification::centred);

    g.setFont (uiFont (10.f, true, 0.3f));
    g.setColour (Palette::dimText);
    g.drawText ("PRESET", Rectangle<int> (W - 16 - 230, 16, 230, 14), Justification::left);
    g.drawText ("v1.0  //  MONO  LEGATO  ENGINE", Rectangle<int> (16, 34, 300, 14), Justification::left);
    g.drawText ("4X OVERSAMPLED", Rectangle<int> (16, 50, 300, 14), Justification::left);
    g.setColour (Palette::red.withAlpha (0.8f));
    g.drawText ("ENTER THE PROPHECY", Rectangle<int> (16, 66, 300, 14), Justification::left);

    // header rule
    ColourGradient rule (Palette::red.withAlpha (0.f), 0.f, 0.f, Palette::red.withAlpha (0.f), (float) W, 0.f, false);
    rule.addColour (0.5, Palette::red.withAlpha (0.9f));
    g.setGradientFill (rule);
    g.fillRect (0, 108, W, 2);

    // panels
    for (auto& s : sections)
    {
        auto r = s.area.toFloat();
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.f, 3.f), 8.f);
        ColourGradient pg (Colour (0xff140d10), r.getX(), r.getY(), Palette::panel, r.getX(), r.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (Palette::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);

        // title strip
        auto t = s.area.reduced (12, 8).removeFromTop (20);
        Path diamond;
        const float dx = (float) t.getX() + 4.f, dy = (float) t.getCentreY();
        diamond.addQuadrilateral (dx, dy - 4.f, dx + 4.f, dy, dx, dy + 4.f, dx - 4.f, dy);
        g.setColour (Palette::red);
        g.fillPath (diamond);
        g.setFont (uiFont (12.f, true, 0.35f));
        g.drawText (s.title, t.withTrimmedLeft (14), Justification::centredLeft);

        ColourGradient ul (Palette::red.withAlpha (0.5f), r.getX(), 0.f, Palette::red.withAlpha (0.f), r.getRight(), 0.f, false);
        g.setGradientFill (ul);
        g.fillRect (Rectangle<float> (r.getX() + 12.f, (float) t.getBottom() + 3.f, r.getWidth() - 24.f, 1.f));
    }
}

// ====================================================================== Editor
ProphecyEditor::ProphecyEditor (ProphecyProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (768, 456, 1920, 1140);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1088, 646);
}

void ProphecyEditor::resized()
{
    const float s = (float) getWidth() / (float) Canvas::W;
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale (s));
}
