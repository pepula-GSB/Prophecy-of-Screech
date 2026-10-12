#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------ text formatting
namespace
{
    String fmtHz (float v, int)    { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; }
    String fmtMs (float v, int)    { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : String (v, v < 10.f ? 1 : 0) + " ms"; }
    String fmtDb (float v, int)    { return String (v, 1) + " dB"; }
    String fmtPct (float v, int)   { return String ((int) std::round (v * 100.f)) + "%"; }
    String fmtSemi (float v, int)  { return (v > 0.f ? "+" : "") + String (v, 1) + " st"; }
    String fmtOct (float v, int)   { return (v > 0.f ? "+" : "") + String (v, 2) + " oct"; }
    String fmtCents (float v, int) { return String ((int) std::round (v)) + " ct"; }
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

    std::unique_ptr<AudioParameterFloat> floatParam (const char* id, const char* name, NormalisableRange<float> range, float def,
                                                     String (*fmt) (float, int), float (*parse) (const String&))
    {
        auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withValueFromStringFunction (parse);
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def, attrs);
    }
}

AudioProcessorValueTreeState::ParameterLayout ProphecyProcessor::createLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout l;

    // OSC
    l.add (floatParam (wave,   "Wave (Saw>Square)", { 0.f, 1.f }, 0.f, fmtPct, parsePct));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { unison, 1 }, "Unison", 1, 7, 3));
    l.add (floatParam (detune, "Detune", { 0.f, 100.f }, 18.f, fmtCents, parseNum));
    l.add (floatParam (width,  "Stereo Width", { 0.f, 1.f }, 0.6f, fmtPct, parsePct));
    l.add (floatParam (fm,     "FM Amount", { 0.f, 1.f }, 0.25f, fmtPct, parsePct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { fmRatio, 1 }, "FM Ratio", ParamChoices::fmRatios, 1));

    // PITCH
    l.add (floatParam (glide,  "Glide", skewed (0.f, 1000.f, 120.f), 60.f, fmtMs, parseMs));
    l.add (floatParam (penv,   "Pitch Env", { -48.f, 48.f, 0.1f }, 12.f, fmtSemi, parseNum));
    l.add (floatParam (pdecay, "Pitch Decay", skewed (1.f, 2000.f, 150.f), 120.f, fmtMs, parseMs));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { bend, 1 }, "Bend Range", 1, 24, 12));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { legato, 1 }, "Legato", true));

    // MACRO
    l.add (floatParam (scream, "SCREAM", { 0.f, 1.f }, 0.3f, fmtPct, parsePct));

    // FILTER
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ftype, 1 }, "Filter Type", ParamChoices::filterTypes, 1));
    l.add (floatParam (cutoff,   "Cutoff", skewed (20.f, 20000.f, 1000.f), 3500.f, fmtHz, parseHz));
    l.add (floatParam (reso,     "Resonance", { 0.f, 1.f }, 0.45f, fmtPct, parsePct));
    l.add (floatParam (fenv,     "Filter Env", { -6.f, 6.f }, 2.5f, fmtOct, parseNum));
    l.add (floatParam (fdecay,   "Filter Decay", skewed (1.f, 3000.f, 300.f), 300.f, fmtMs, parseMs));
    l.add (floatParam (keytrack, "Key Track", { 0.f, 1.f }, 0.5f, fmtPct, parsePct));

    // DISTORTION
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { dtype, 1 }, "Dist Type", ParamChoices::distTypes, 3));
    l.add (floatParam (drive, "Drive", { 0.f, 48.f }, 24.f, fmtDb, parseNum));
    l.add (floatParam (bite,  "Bite (Pre HP)", skewed (20.f, 2000.f, 250.f), 250.f, fmtHz, parseHz));
    l.add (floatParam (dmix,  "Dist Mix", { 0.f, 1.f }, 1.f, fmtPct, parsePct));

    // LFO
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { lshape, 1 }, "LFO Shape", ParamChoices::lfoShapes, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { lsync, 1 }, "LFO Sync", true));
    l.add (floatParam (lrate,  "LFO Rate", skewed (0.05f, 30.f, 4.f), 4.f, fmtLfoHz, parseNum));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { lfoDiv, 1 }, "LFO Division", ParamChoices::divisions, 5));
    l.add (floatParam (lcut,   "LFO > Cutoff", { -4.f, 4.f }, 0.f, fmtOct, parseNum));
    l.add (floatParam (lpitch, "LFO > Pitch", { -12.f, 12.f }, 0.f, fmtSemi, parseNum));

    // AMP
    l.add (floatParam (attack,  "Attack", skewed (0.1f, 2000.f, 50.f), 2.f, fmtMs, parseMs));
    l.add (floatParam (decay,   "Decay", skewed (1.f, 4000.f, 400.f), 400.f, fmtMs, parseMs));
    l.add (floatParam (sustain, "Sustain", { 0.f, 1.f }, 1.f, fmtPct, parsePct));
    l.add (floatParam (release, "Release", skewed (1.f, 5000.f, 300.f), 150.f, fmtMs, parseMs));

    // DELAY
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { dtime, 1 }, "Delay Time", ParamChoices::divisions, 6));
    l.add (floatParam (dfb, "Delay Feedback", { 0.f, 0.95f }, 0.35f, fmtPct, parsePct));
    l.add (floatParam (dfx, "Delay Mix", { 0.f, 1.f }, 0.15f, fmtPct, parsePct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pingpong, 1 }, "Ping Pong", true));

    // REVERB
    l.add (floatParam (rsize, "Reverb Size", { 0.f, 1.f }, 0.75f, fmtPct, parsePct));
    l.add (floatParam (rdamp, "Reverb Damp", { 0.f, 1.f }, 0.4f, fmtPct, parsePct));
    l.add (floatParam (rmix,  "Reverb Mix", { 0.f, 1.f }, 0.2f, fmtPct, parsePct));

    // MASTER
    l.add (floatParam (volume, "Volume", { -36.f, 6.f }, -6.f, fmtDb, parseNum));

    return l;
}

// ------------------------------------------------------------ presets
namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

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

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// ------------------------------------------------------------ processor
ProphecyProcessor::ProphecyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PROPHECY", createLayout())
{
    using namespace ParamIDs;
    auto g = [this] (const char* id) { auto* r = apvts.getRawParameterValue (id); jassert (r != nullptr); return r; };
    ptr = { g (wave), g (unison), g (detune), g (width), g (fm), g (fmRatio),
            g (glide), g (penv), g (pdecay), g (bend), g (legato), g (scream),
            g (ftype), g (cutoff), g (reso), g (fenv), g (fdecay), g (keytrack),
            g (dtype), g (drive), g (bite), g (dmix),
            g (lshape), g (lsync), g (lrate), g (lfoDiv), g (lcut), g (lpitch),
            g (attack), g (decay), g (sustain), g (release),
            g (dtime), g (dfb), g (dfx), g (pingpong), g (rsize), g (rdamp), g (rmix), g (volume) };
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

    EngineParams e;
    e.wave = ptr.wave->load();
    e.unison = idx (ptr.unison, 7);
    e.detuneCents = ptr.detune->load();
    e.width = ptr.width->load();
    e.fm = ptr.fm->load();
    e.fmRatio = ParamChoices::fmRatioValues[idx (ptr.fmRatio, 7)];
    e.glideMs = ptr.glide->load();
    e.penvSemis = ptr.penv->load();
    e.pdecayMs = ptr.pdecay->load();
    e.bendRange = idx (ptr.bend, 24);
    e.legato = ptr.legato->load() > 0.5f;
    e.scream = ptr.scream->load();
    e.ftype = idx (ptr.ftype, 3);
    e.cutoff = ptr.cutoff->load();
    e.reso = ptr.reso->load();
    e.fenvOct = ptr.fenv->load();
    e.fdecayMs = ptr.fdecay->load();
    e.keytrack = ptr.keytrack->load();
    e.dtype = idx (ptr.dtype, 3);
    e.driveDb = ptr.drive->load();
    e.biteHz = ptr.bite->load();
    e.dmix = ptr.dmix->load();
    e.lshape = idx (ptr.lshape, 4);

    const bool sync = ptr.lsync->load() > 0.5f;
    const float beats = ParamChoices::divisionBeats[idx (ptr.lfoDiv, 12)];
    e.lfoHz = sync ? (float) (bpm / 60.0 / beats) : ptr.lrate->load();
    e.lcutOct = ptr.lcut->load();
    e.lpitchSemis = ptr.lpitch->load();

    e.attackMs = ptr.attack->load();
    e.decayMs = ptr.decay->load();
    e.sustain = ptr.sustain->load();
    e.releaseMs = ptr.release->load();

    e.delayBeats = ParamChoices::divisionBeats[idx (ptr.dtime, 12)];
    e.delayFb = ptr.dfb->load();
    e.delayMix = ptr.dfx->load();
    e.pingpong = ptr.pingpong->load() > 0.5f;
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

    // scope feed
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
            apvts.replaceState (vt);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ProphecyProcessor(); }
