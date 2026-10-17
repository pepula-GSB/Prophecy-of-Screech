#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------ text formatting
namespace
{
    String fmtHz (float v, int)    { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; }
    String fmtMs (float v, int)    { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : String (v, v < 10.f ? 1 : 0) + " ms"; }
    String fmtDb (float v, int)    { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String fmtPct (float v, int)   { return String ((int) std::round (v * 100.f)) + "%"; }
    String fmtSemi (float v, int)  { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; }
    String fmtOct (float v, int)   { return (v > 0.005f ? "+" : "") + String (v, 2) + " oct"; }
    String fmtCents (float v, int) { return (v > 0.5f ? "+" : "") + String ((int) std::round (v)) + " ct"; }
    String fmtDetune (float v, int){ return String ((int) std::round (v)) + " ct"; }
    String fmtLfoHz (float v, int) { return String (v, v < 10.f ? 2 : 1) + " Hz"; }
    float  parseNum (const String& t) { return t.retainCharacters ("0123456789.-+").getFloatValue(); }
    float  parseHz (const String& t)  { auto v = parseNum (t); return t.containsIgnoreCase ("k") ? v * 1000.f : v; }
    float  parseMs (const String& t)  { auto v = parseNum (t); return (t.trim().endsWithIgnoreCase ("s") && ! t.containsIgnoreCase ("ms")) ? v * 1000.f : v; }
    float  parsePct (const String& t) { return parseNum (t) / 100.f; }

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    // v1 parameters keep version hint 1, parameters added in v2 use 2
    int versionOf (const String& id) { return (id.startsWith ("o1_") || id.startsWith ("o2_") || id == "dly_on" || id == "rev_on") ? 2 : 1; }

    std::unique_ptr<AudioParameterFloat> floatParam (const String& id, const String& name, NormalisableRange<float> range, float def,
                                                     String (*fmt) (float, int), float (*parse) (const String&))
    {
        auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withValueFromStringFunction (parse);
        return std::make_unique<AudioParameterFloat> (ParameterID { id, versionOf (id) }, name, range, def, attrs);
    }
    std::unique_ptr<AudioParameterBool> boolParam (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, versionOf (id) }, name, def);
    }
    std::unique_ptr<AudioParameterChoice> choiceParam (const String& id, const String& name, const StringArray& c, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, versionOf (id) }, name, c, def);
    }
    std::unique_ptr<AudioParameterInt> intParam (const String& id, const String& name, int lo, int hi, int def)
    {
        return std::make_unique<AudioParameterInt> (ParameterID { id, versionOf (id) }, name, lo, hi, def);
    }
}

AudioProcessorValueTreeState::ParameterLayout ProphecyProcessor::createLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout l;

    for (int o = 0; o < numOscs; ++o)
    {
        const String n = "OSC" + String (o + 1) + " ";
        auto id = [o] (const char* base) { return ParamIDs::osc (o, base); };

        // oscillator
        l.add (boolParam  (id (on),      n + "On", o == 0));
        l.add (floatParam (id (level),   n + "Level", { -36.f, 6.f }, o == 0 ? 0.f : -3.f, fmtDb, parseNum));
        l.add (intParam   (id (oct),     n + "Octave", -3, 3, o == 0 ? 0 : -1));
        l.add (floatParam (id (semi),    n + "Semi", { -12.f, 12.f, 1.f }, 0.f, fmtSemi, parseNum));
        l.add (floatParam (id (fine),    n + "Fine", { -100.f, 100.f }, 0.f, fmtCents, parseNum));
        l.add (floatParam (id (wave),    n + "Wave (Saw>Square)", { 0.f, 1.f }, o == 0 ? 0.f : 0.5f, fmtPct, parsePct));
        l.add (intParam   (id (unison),  n + "Unison", 1, 7, 3));
        l.add (floatParam (id (detune),  n + "Detune", { 0.f, 100.f }, 18.f, fmtDetune, parseNum));
        l.add (floatParam (id (width),   n + "Stereo Width", { 0.f, 1.f }, 0.6f, fmtPct, parsePct));
        l.add (floatParam (id (fm),      n + "FM Amount", { 0.f, 1.f }, 0.25f, fmtPct, parsePct));
        l.add (choiceParam (id (fmRatio), n + "FM Ratio", ParamChoices::fmRatios, 1));
        l.add (floatParam (id (penv),    n + "Pitch Env", { -48.f, 48.f, 0.1f }, 12.f, fmtSemi, parseNum));
        l.add (floatParam (id (pdecay),  n + "Pitch Decay", skewed (1.f, 2000.f, 150.f), 120.f, fmtMs, parseMs));

        // distortion
        l.add (boolParam  (id (distOn),  n + "Dist On", true));
        l.add (choiceParam (id (dtype),  n + "Dist Type", ParamChoices::distTypes, 3));
        l.add (floatParam (id (drive),   n + "Drive", { 0.f, 48.f }, 24.f, fmtDb, parseNum));
        l.add (floatParam (id (bite),    n + "Bite (Pre HP)", skewed (20.f, 2000.f, 250.f), 250.f, fmtHz, parseHz));
        l.add (floatParam (id (dmix),    n + "Dist Mix", { 0.f, 1.f }, 1.f, fmtPct, parsePct));

        // filter
        l.add (boolParam  (id (filtOn),  n + "Filter On", true));
        l.add (choiceParam (id (ftype),  n + "Filter Type", ParamChoices::filterTypes, 1));
        l.add (floatParam (id (cutoff),  n + "Cutoff", skewed (20.f, 20000.f, 1000.f), 3500.f, fmtHz, parseHz));
        l.add (floatParam (id (reso),    n + "Resonance", { 0.f, 1.f }, 0.45f, fmtPct, parsePct));
        l.add (floatParam (id (fenv),    n + "Filter Env", { -6.f, 6.f }, 2.5f, fmtOct, parseNum));
        l.add (floatParam (id (fdecay),  n + "Filter Decay", skewed (1.f, 3000.f, 300.f), 300.f, fmtMs, parseMs));
        l.add (floatParam (id (keytrack), n + "Key Track", { 0.f, 1.f }, 0.5f, fmtPct, parsePct));

        // lfo
        l.add (boolParam  (id (lfoOn),   n + "LFO On", true));
        l.add (choiceParam (id (lshape), n + "LFO Shape", ParamChoices::lfoShapes, 0));
        l.add (boolParam  (id (lsync),   n + "LFO Sync", true));
        l.add (floatParam (id (lrate),   n + "LFO Rate", skewed (0.05f, 30.f, 4.f), 4.f, fmtLfoHz, parseNum));
        l.add (choiceParam (id (lfoDiv), n + "LFO Division", ParamChoices::divisions, 5));
        l.add (floatParam (id (lcut),    n + "LFO > Cutoff", { -4.f, 4.f }, 0.f, fmtOct, parseNum));
        l.add (floatParam (id (lpitch),  n + "LFO > Pitch", { -12.f, 12.f }, 0.f, fmtSemi, parseNum));
    }

    // VOICE
    l.add (floatParam (glide, "Glide", skewed (0.f, 1000.f, 120.f), 60.f, fmtMs, parseMs));
    l.add (intParam   (bend, "Bend Range", 1, 24, 12));
    l.add (boolParam  (legato, "Legato", true));
    l.add (floatParam (scream, "SCREAM", { 0.f, 1.f }, 0.3f, fmtPct, parsePct));

    // AMP
    l.add (floatParam (attack,  "Attack", skewed (0.1f, 2000.f, 50.f), 2.f, fmtMs, parseMs));
    l.add (floatParam (decay,   "Decay", skewed (1.f, 4000.f, 400.f), 400.f, fmtMs, parseMs));
    l.add (floatParam (sustain, "Sustain", { 0.f, 1.f }, 1.f, fmtPct, parsePct));
    l.add (floatParam (release, "Release", skewed (1.f, 5000.f, 300.f), 150.f, fmtMs, parseMs));

    // DELAY
    l.add (boolParam  (dlyOn, "Delay On", true));
    l.add (choiceParam (dtime, "Delay Time", ParamChoices::divisions, 6));
    l.add (floatParam (dfb, "Delay Feedback", { 0.f, 0.95f }, 0.35f, fmtPct, parsePct));
    l.add (floatParam (dfx, "Delay Mix", { 0.f, 1.f }, 0.15f, fmtPct, parsePct));
    l.add (boolParam  (pingpong, "Ping Pong", true));

    // REVERB
    l.add (boolParam  (revOn, "Reverb On", true));
    l.add (floatParam (rsize, "Reverb Size", { 0.f, 1.f }, 0.75f, fmtPct, parsePct));
    l.add (floatParam (rdamp, "Reverb Damp", { 0.f, 1.f }, 0.4f, fmtPct, parsePct));
    l.add (floatParam (rmix,  "Reverb Mix", { 0.f, 1.f }, 0.2f, fmtPct, parsePct));

    // MASTER
    l.add (floatParam (volume, "Volume", { -36.f, 6.f }, -6.f, fmtDb, parseNum));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct Preset { const char* name; std::vector<std::pair<String, float>> values; };

    String o2 (const char* base) { return ParamIDs::osc (1, base); }

    const std::vector<Preset>& presets()
    {
        using namespace ParamIDs;
        static const std::vector<Preset> p {
            { "Prophecy Lead", {} },
            { "Raw Scream", { { wave, 0.2f }, { unison, 5 }, { detune, 25 }, { fm, 0.55f }, { fmRatio, 3 /*2*/ },
                              { penv, 24 }, { pdecay, 80 }, { scream, 0.6f }, { ftype, 1 }, { cutoff, 2500 }, { reso, 0.6f },
                              { fenv, 3 }, { fdecay, 220 }, { dtype, 3 }, { drive, 30 }, { lfoDiv, 2 /*1/16*/ }, { lcut, 1.2f } } },
            { "Talking Screech", { { fm, 0.35f }, { fmRatio, 1 }, { ftype, 2 }, { cutoff, 1200 }, { reso, 0.75f }, { fenv, 0 },
                                   { lshape, 0 }, { lfoDiv, 5 /*1/8*/ }, { lcut, 2.0f }, { dtype, 1 }, { drive, 20 }, { scream, 0.2f } } },
            { "Siren Riser", { { glide, 400 }, { penv, -24 }, { pdecay, 900 }, { lshape, 1 }, { lfoDiv, 8 /*1/4*/ }, { lpitch, 2 },
                               { fm, 0.2f }, { cutoff, 4000 }, { reso, 0.4f }, { fenv, 1 }, { fdecay, 800 }, { rmix, 0.3f } } },
            { "Uptempo Laser", { { penv, 36 }, { pdecay, 40 }, { fm, 0.7f }, { fmRatio, 4 /*3*/ }, { dtype, 0 }, { drive, 36 },
                                 { cutoff, 6000 }, { reso, 0.3f }, { fenv, 1.5f }, { fdecay, 120 }, { unison, 1 }, { release, 60 },
                                 { dtime, 2 /*1/16*/ }, { dfb, 0.3f }, { glide, 20 } } },
            { "Euphoric Scream", { { unison, 7 }, { detune, 30 }, { wave, 0.5f }, { fm, 0.15f }, { dtype, 1 }, { drive, 18 },
                                   { cutoff, 4500 }, { reso, 0.35f }, { rmix, 0.35f }, { dfx, 0.25f }, { scream, 0.15f },
                                   { width, 0.9f } } },
            { "Frenchcore Rip", { { dtype, 2 }, { drive, 40 }, { fm, 0.8f }, { fmRatio, 2 /*1.5*/ }, { lshape, 4 }, { lfoDiv, 2 },
                                  { lcut, 2.5f }, { ftype, 0 }, { cutoff, 3000 }, { reso, 0.55f }, { scream, 0.5f }, { bite, 600 } } },
            { "Hoover Scream", { { unison, 7 }, { detune, 55 }, { wave, 0.35f }, { glide, 180 }, { penv, -12 }, { pdecay, 300 },
                                 { fm, 0.3f }, { fmRatio, 0 /*0.5*/ }, { dtype, 1 }, { drive, 16 }, { ftype, 0 }, { cutoff, 5000 },
                                 { reso, 0.2f }, { width, 1.f } } },
            { "Dutchie", { { wave, 1.f }, { unison, 4 }, { detune, 90 }, { width, 1.f }, { fm, 0.91f }, { fmRatio, 3 /*2*/ },
                           { glide, 116.422f }, { penv, 16.1f }, { pdecay, 321.191f }, { bend, 15 }, { legato, 1 },
                           { dtype, 3 }, { drive, 48 }, { bite, 25 }, { dmix, 1.f },
                           { ftype, 1 }, { cutoff, 20000 }, { reso, 0.51f }, { fenv, 2.5f }, { fdecay, 190.342f }, { keytrack, 1.f },
                           { lshape, 1 }, { lsync, 0 }, { lrate, 4.f }, { lfoDiv, 3 /*1/16D*/ }, { lcut, 0.41f }, { lpitch, 5.f },
                           { attack, 0.1f }, { decay, 1940 }, { sustain, 1.f }, { release, 29.715f },
                           { dtime, 8 /*1/4*/ }, { dfb, 0.52f }, { dfx, 0.29f }, { pingpong, 1 },
                           { rsize, 0.65f }, { rdamp, 0.63f }, { rmix, 0.52f }, { volume, -6 }, { scream, 0.5f } } },

            // ---- early hardstyle (v2)
            { "Zaagtand '04", { { wave, 0.11f }, { unison, 6 }, { detune, 67 }, { width, 0.49f }, { fm, 0.17f }, { fmRatio, 6 /*5*/ },
                                { glide, 255.795f }, { penv, 12.5f }, { pdecay, 441.823f }, { bend, 12 }, { legato, 1 },
                                { dtype, 3 }, { drive, 14.2f }, { bite, 45 }, { dmix, 0.94f },
                                { ftype, 1 }, { cutoff, 8360 }, { reso, 0.21f }, { fenv, 0.68f }, { fdecay, 211.425f }, { keytrack, 0.28f },
                                { lshape, 1 }, { lsync, 1 }, { lrate, 4.06f }, { lfoDiv, 8 /*1/4*/ }, { lcut, 2.06f }, { lpitch, 0 },
                                { scream, 0.43f }, { attack, 0.3f }, { decay, 449.149f }, { sustain, 0.94f }, { release, 170.162f },
                                { dtime, 8 /*1/4*/ }, { dfb, 0.56f }, { dfx, 0.02f }, { pingpong, 1 },
                                { rsize, 0.74f }, { rdamp, 0.47f }, { rmix, 0.39f }, { volume, -6 } } },
            { "Oldskool Gekte '05", { { wave, 0.67f }, { unison, 2 }, { detune, 66 }, { width, 0.97f }, { fm, 0.69f }, { fmRatio, 7 /*7*/ },
                                      { glide, 5.3f }, { penv, 28.2f }, { pdecay, 146.305f }, { bend, 12 }, { legato, 1 },
                                      { dtype, 3 }, { drive, 22.6f }, { bite, 42 }, { dmix, 0.73f },
                                      { ftype, 0 }, { cutoff, 7410 }, { reso, 0.34f }, { fenv, 0.65f }, { fdecay, 146.596f }, { keytrack, 0.68f },
                                      { lshape, 2 }, { lsync, 1 }, { lrate, 1.25f }, { lfoDiv, 5 /*1/8*/ }, { lcut, 2.07f }, { lpitch, 0 },
                                      { scream, 0.36f }, { attack, 0.7f }, { decay, 987.403f }, { sustain, 0.88f }, { release, 36.9869f },
                                      { dtime, 6 /*1/8D*/ }, { dfb, 0.56f }, { dfx, 0.25f }, { pingpong, 1 },
                                      { rsize, 0.5f }, { rdamp, 0.39f }, { rmix, 0.14f }, { volume, -6 } } },
            { "Twin Prophecy", { { wave, 0.15f }, { unison, 5 }, { detune, 35 }, { fm, 0.4f }, { fmRatio, 3 /*2*/ }, { penv, 14 },
                                 { dtype, 3 }, { drive, 26 }, { cutoff, 4200 }, { reso, 0.4f },
                                 { o2 (on), 1 }, { o2 (oct), -1 }, { o2 (level), -4 }, { o2 (wave), 1.f }, { o2 (unison), 3 },
                                 { o2 (detune), 22 }, { o2 (fm), 0.1f }, { o2 (penv), 7 }, { o2 (dtype), 1 }, { o2 (drive), 16 },
                                 { o2 (ftype), 1 }, { o2 (cutoff), 1600 }, { o2 (reso), 0.3f }, { o2 (fenv), 1.5f }, { o2 (lfoOn), 0 },
                                 { glide, 90 }, { rmix, 0.25f } } },
        };
        return p;
    }
}

int ProphecyProcessor::getNumPrograms() { return (int) presets().size(); }

const String ProphecyProcessor::getProgramName (int index)
{
    return isPositiveAndBelow (index, (int) presets().size()) ? String (presets()[(size_t) index].name) : String();
}

void ProphecyProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    currentPresetName = presets()[(size_t) index].name;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// ------------------------------------------------------------ user presets
File ProphecyProcessor::getUserPresetFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory)
               .getChildFile ("Prophecy of Scream").getChildFile ("Presets");
}

Array<File> ProphecyProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (File::findFiles, false, "*.pos");
    files.sort();
    return files;
}

bool ProphecyProcessor::saveUserPreset (const String& name)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;

    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;

    std::unique_ptr<XmlElement> xml (apvts.copyState().createXml());
    if (xml == nullptr) return false;
    xml->removeAttribute ("program");
    xml->removeAttribute ("presetName");

    if (! xml->writeTo (folder.getChildFile (clean + ".pos")))
        return false;

    currentPresetName = clean;
    return true;
}

void ProphecyProcessor::applyValues (const XmlElement& xml)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (child->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child->getDoubleAttribute ("value")));
}

bool ProphecyProcessor::loadUserPreset (const File& file)
{
    auto xml = XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    applyValues (*xml);
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

// ------------------------------------------------------------ randomize
void ProphecyProcessor::randomize()
{
    using namespace ParamIDs;
    auto& r = randomGen;
    auto set = [this] (const String& id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    auto between = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };
    auto logBetween = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
    auto pick = [&r] (std::initializer_list<int> opts) { return (float) *(opts.begin() + r.nextInt ((int) opts.size())); };
    auto chance = [&r] (float pr) { return r.nextFloat() < pr ? 1.f : 0.f; };

    for (int o = 0; o < numOscs; ++o)
    {
        auto id = [o] (const char* base) { return ParamIDs::osc (o, base); };
        const bool main = o == 0;

        set (id (on), main ? 1.f : chance (0.45f));
        set (id (level), main ? 0.f : between (-9.f, -2.f));
        set (id (oct), main ? 0.f : pick ({ -1, -1, 0, 1 }));
        set (id (semi), main ? 0.f : pick ({ 0, 0, 0, 7, -5, 12 }));
        set (id (fine), main ? 0.f : between (-12.f, 12.f));
        set (id (wave), r.nextFloat());
        set (id (unison), (float) (1 + r.nextInt (7)));
        set (id (detune), between (5.f, 70.f));
        set (id (width), between (0.3f, 1.f));
        set (id (fm), between (0.f, main ? 0.9f : 0.6f));
        set (id (fmRatio), (float) r.nextInt (8));
        set (id (penv), r.nextFloat() < 0.75f ? between (4.f, 36.f) : between (-24.f, -4.f));
        set (id (pdecay), logBetween (30.f, 600.f));

        set (id (distOn), main ? 1.f : chance (0.85f));
        set (id (dtype), (float) r.nextInt (4));
        set (id (drive), between (14.f, 44.f));
        set (id (bite), logBetween (40.f, 900.f));
        set (id (dmix), between (0.7f, 1.f));

        set (id (filtOn), chance (0.9f));
        set (id (ftype), pick ({ 0, 1, 1, 2 }));
        set (id (cutoff), logBetween (900.f, 14000.f));
        set (id (reso), between (0.15f, 0.8f));
        set (id (fenv), between (-1.f, 4.f));
        set (id (fdecay), logBetween (60.f, 1200.f));
        set (id (keytrack), between (0.2f, 1.f));

        set (id (lfoOn), chance (0.7f));
        set (id (lshape), (float) r.nextInt (5));
        set (id (lsync), chance (0.8f));
        set (id (lrate), logBetween (0.5f, 12.f));
        set (id (lfoDiv), pick ({ 2, 3, 4, 5, 6, 8 }));
        set (id (lcut), r.nextFloat() < 0.7f ? between (-0.5f, 2.5f) : 0.f);
        set (id (lpitch), r.nextFloat() < 0.25f ? between (-2.f, 2.f) : 0.f);
    }

    set (glide, logBetween (5.f, 300.f));
    set (legato, 1.f);
    set (scream, between (0.1f, 0.7f));
    set (attack, logBetween (0.1f, 15.f));
    set (decay, logBetween (150.f, 2500.f));
    set (sustain, between (0.6f, 1.f));
    set (release, logBetween (20.f, 400.f));
    set (dlyOn, chance (0.8f));
    set (dtime, pick ({ 2, 5, 6, 8 }));
    set (dfb, between (0.1f, 0.6f));
    set (dfx, between (0.f, 0.3f));
    set (revOn, chance (0.9f));
    set (rsize, between (0.4f, 0.9f));
    set (rdamp, between (0.2f, 0.7f));
    set (rmix, between (0.05f, 0.4f));
    // volume is left alone on purpose

    currentPresetName = "Random #" + String (1000 + r.nextInt (9000));
}

// ------------------------------------------------------------ processor
ProphecyProcessor::ProphecyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PROPHECY", createLayout())
{
    using namespace ParamIDs;
    auto g = [this] (const String& id) { auto* r = apvts.getRawParameterValue (id); jassert (r != nullptr); return r; };

    for (int o = 0; o < numOscs; ++o)
    {
        auto& P = ptr.osc[(size_t) o];
        auto id = [o] (const char* base) { return ParamIDs::osc (o, base); };
        P.on = g (id (on)); P.level = g (id (level)); P.oct = g (id (oct)); P.semi = g (id (semi)); P.fine = g (id (fine));
        P.wave = g (id (wave)); P.unison = g (id (unison)); P.detune = g (id (detune)); P.width = g (id (width));
        P.fm = g (id (fm)); P.fmRatio = g (id (fmRatio)); P.penv = g (id (penv)); P.pdecay = g (id (pdecay));
        P.distOn = g (id (distOn)); P.dtype = g (id (dtype)); P.drive = g (id (drive)); P.bite = g (id (bite)); P.dmix = g (id (dmix));
        P.filtOn = g (id (filtOn)); P.ftype = g (id (ftype)); P.cutoff = g (id (cutoff)); P.reso = g (id (reso));
        P.fenv = g (id (fenv)); P.fdecay = g (id (fdecay)); P.keytrack = g (id (keytrack));
        P.lfoOn = g (id (lfoOn)); P.lshape = g (id (lshape)); P.lsync = g (id (lsync)); P.lrate = g (id (lrate));
        P.lfoDiv = g (id (lfoDiv)); P.lcut = g (id (lcut)); P.lpitch = g (id (lpitch));
    }
    ptr.glide = g (glide); ptr.bend = g (bend); ptr.legato = g (legato); ptr.scream = g (scream);
    ptr.attack = g (attack); ptr.decay = g (decay); ptr.sustain = g (sustain); ptr.release = g (release);
    ptr.dlyOn = g (dlyOn); ptr.dtime = g (dtime); ptr.dfb = g (dfb); ptr.dfx = g (dfx); ptr.pingpong = g (pingpong);
    ptr.revOn = g (revOn); ptr.rsize = g (rsize); ptr.rdamp = g (rdamp); ptr.rmix = g (rmix); ptr.volume = g (volume);

    // fresh instances start on "Dutchie"
    for (int i = 0; i < getNumPrograms(); ++i)
        if (getProgramName (i) == "Dutchie") { setCurrentProgram (i); break; }
}

bool ProphecyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void ProphecyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gatherParams (150.0));
    engine.prepare (sampleRate, samplesPerBlock);
}

EngineParams ProphecyProcessor::gatherParams (double bpm) const
{
    auto idx = [] (std::atomic<float>* a, int maxIdx) { return jlimit (0, maxIdx, (int) std::round (a->load())); };
    auto on = [] (std::atomic<float>* a) { return a->load() > 0.5f; };

    EngineParams e;
    for (int o = 0; o < ParamIDs::numOscs; ++o)
    {
        const auto& P = ptr.osc[(size_t) o];
        auto& O = e.osc[(size_t) o];
        O.on = on (P.on);
        O.levelDb = P.level->load();
        O.octave = (int) std::round (P.oct->load());
        O.semi = std::round (P.semi->load());
        O.fineCents = P.fine->load();
        O.wave = P.wave->load();
        O.unison = idx (P.unison, 7);
        O.detuneCents = P.detune->load();
        O.width = P.width->load();
        O.fm = P.fm->load();
        O.fmRatio = ParamChoices::fmRatioValues[idx (P.fmRatio, 7)];
        O.penvSemis = P.penv->load();
        O.pdecayMs = P.pdecay->load();

        O.distOn = on (P.distOn);
        O.dtype = idx (P.dtype, 3);
        O.driveDb = P.drive->load();
        O.biteHz = P.bite->load();
        O.dmix = P.dmix->load();

        O.filtOn = on (P.filtOn);
        O.ftype = idx (P.ftype, 3);
        O.cutoff = P.cutoff->load();
        O.reso = P.reso->load();
        O.fenvOct = P.fenv->load();
        O.fdecayMs = P.fdecay->load();
        O.keytrack = P.keytrack->load();

        O.lfoOn = on (P.lfoOn);
        O.lshape = idx (P.lshape, 4);
        const float beats = ParamChoices::divisionBeats[idx (P.lfoDiv, 12)];
        O.lfoHz = on (P.lsync) ? (float) (bpm / 60.0 / beats) : P.lrate->load();
        O.lcutOct = P.lcut->load();
        O.lpitchSemis = P.lpitch->load();
    }

    e.glideMs = ptr.glide->load();
    e.bendRange = idx (ptr.bend, 24);
    e.legato = on (ptr.legato);
    e.scream = ptr.scream->load();
    e.attackMs = ptr.attack->load();
    e.decayMs = ptr.decay->load();
    e.sustain = ptr.sustain->load();
    e.releaseMs = ptr.release->load();
    e.delayOn = on (ptr.dlyOn);
    e.delayBeats = ParamChoices::divisionBeats[idx (ptr.dtime, 12)];
    e.delayFb = ptr.dfb->load();
    e.delayMix = ptr.dfx->load();
    e.pingpong = on (ptr.pingpong);
    e.reverbOn = on (ptr.revOn);
    e.revSize = ptr.rsize->load();
    e.revDamp = ptr.rdamp->load();
    e.revMix = ptr.rmix->load();
    e.volumeDb = ptr.volume->load();
    e.bpm = bpm;
    return e;
}

void ProphecyProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;

    double bpm = 150.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    engine.setParams (gatherParams (bpm));
    engine.process (buffer, midi);

    const float* L = buffer.getReadPointer (0);
    const float* R = buffer.getReadPointer (1);
    int w = scopeWritePos.load (std::memory_order_relaxed);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        scopeBuffer[(size_t) w] = 0.5f * (L[i] + R[i]);
        w = (w + 1) & (scopeSize - 1);
    }
    scopeWritePos.store (w, std::memory_order_release);
}

AudioProcessorEditor* ProphecyProcessor::createEditor() { return new ProphecyEditor (*this); }

void ProphecyProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("version", 2, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ProphecyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();

            // states saved by v1.0 have no OSC 2 / FX on-off parameters: give them their defaults
            // (otherwise APVTS would keep whatever this instance had before)
            for (auto* prm : getParameters())
                if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                    if (! vt.getChildWithProperty ("id", rp->paramID).isValid())
                    {
                        ValueTree child ("PARAM");
                        child.setProperty ("id", rp->paramID, nullptr);
                        child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                        vt.appendChild (child, nullptr);
                    }

            apvts.replaceState (vt);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ProphecyProcessor(); }
