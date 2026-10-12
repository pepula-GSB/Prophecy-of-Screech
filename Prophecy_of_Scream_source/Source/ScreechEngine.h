#pragma once
// =====================================================================
//  PROPHECY OF SCREAM  -  hardstyle screech engine
//  Mono / legato synth:  unison saw<->square + phase-mod  ->  bite HP
//  -> distortion (4 modes)  ->  SVF filter  ->  amp env
//  (all of the above runs 4x oversampled)  ->  delay  ->  reverb
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>
#include <memory>

struct EngineParams
{
    // OSC
    float wave = 0.f, detuneCents = 18.f, width = 0.6f, fm = 0.25f, fmRatio = 1.f;
    int   unison = 3;
    // PITCH
    float glideMs = 60.f, penvSemis = 12.f, pdecayMs = 120.f;
    int   bendRange = 12;
    bool  legato = true;
    // MACRO
    float scream = 0.3f;
    // FILTER
    int   ftype = 1;
    float cutoff = 3500.f, reso = 0.45f, fenvOct = 2.5f, fdecayMs = 300.f, keytrack = 0.5f;
    // DIST
    int   dtype = 3;
    float driveDb = 24.f, biteHz = 250.f, dmix = 1.f;
    // LFO
    int   lshape = 0;
    float lfoHz = 4.f, lcutOct = 0.f, lpitchSemis = 0.f;
    // AMP
    float attackMs = 2.f, decayMs = 400.f, sustain = 1.f, releaseMs = 150.f;
    // FX
    float delayBeats = 0.75f, delayFb = 0.35f, delayMix = 0.15f;
    bool  pingpong = true;
    float revSize = 0.75f, revDamp = 0.4f, revMix = 0.2f;
    // MASTER
    float volumeDb = -6.f;
    double bpm = 150.0;
};

class ScreechEngine
{
public:
    static constexpr int maxUnison = 7;
    static constexpr int osFactorLog2 = 2;          // 4x oversampling
    static constexpr int osFactor = 1 << osFactorLog2;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        osr = sampleRate * osFactor;
        maxBlockSize = juce::jmax (16, maxBlock);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) maxBlockSize);

        const double smoothOs = 0.015;
        sCutoff.reset (osr, smoothOs);   sCutoff.setCurrentAndTargetValue (p.cutoff);
        sReso.reset (osr, smoothOs);     sReso.setCurrentAndTargetValue (p.reso);
        sDrive.reset (osr, smoothOs);    sDrive.setCurrentAndTargetValue (1.f);
        sDMix.reset (osr, smoothOs);     sDMix.setCurrentAndTargetValue (p.dmix);
        sFm.reset (osr, smoothOs);       sFm.setCurrentAndTargetValue (p.fm);
        sWave.reset (osr, smoothOs);     sWave.setCurrentAndTargetValue (p.wave);
        sBite.reset (osr, smoothOs);     sBite.setCurrentAndTargetValue (p.biteHz);

        sVolume.reset (sr, 0.03);        sVolume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.volumeDb));
        sDelayMix.reset (sr, 0.03);      sDelayMix.setCurrentAndTargetValue (p.delayMix);
        sRevMix.reset (sr, 0.03);        sRevMix.setCurrentAndTargetValue (p.revMix);
        sDelayTime.reset (sr, 0.08);     sDelayTime.setCurrentAndTargetValue ((float) (sr * 0.25));

        delaySize = (int) (sr * 8.5) + 4;
        delayL.assign ((size_t) delaySize, 0.f);
        delayR.assign ((size_t) delaySize, 0.f);
        delayWrite = 0;

        revScratchL.assign ((size_t) maxBlockSize, 0.f);
        revScratchR.assign ((size_t) maxBlockSize, 0.f);

        reverb.setSampleRate (sr);
        reverb.reset();

        reset();
    }

    void reset()
    {
        heldNotes.clear();
        gate = false;
        amp.stage = Env::Idle; amp.value = 0.f;
        fEnv = 0.f; pEnv = 0.f;
        for (auto& c : filt) c = {};
        for (auto& h : biteState) h = 0.f;
        for (auto& v : fbLp) v = 0.f;
        for (auto& v : fbHp) v = 0.f;
        for (auto& v : revHp) v = 0.f;
        std::fill (delayL.begin(), delayL.end(), 0.f);
        std::fill (delayR.begin(), delayR.end(), 0.f);
        reverb.reset();
        if (oversampler) oversampler->reset();
        pitchWheel = 0.f; modWheel = 0.f;
    }

    void setParams (const EngineParams& np) { p = np; }

    // ---------------------------------------------------------------
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        const int numSamples = buffer.getNumSamples();
        updateBlockTargets();

        int pos = 0;
        for (const auto meta : midi)
        {
            const int evPos = juce::jlimit (0, numSamples, meta.samplePosition);
            if (evPos > pos) { renderChunked (buffer, pos, evPos - pos); pos = evPos; }
            handleMidi (meta.getMessage());
        }
        if (pos < numSamples) renderChunked (buffer, pos, numSamples - pos);
    }

private:
    // ------------------------------- envelopes
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float value = 0.f, attackInc = 0.f, decCoef = 0.f, relCoef = 0.f, sustain = 1.f;

        void trigger (bool fromZero) { if (fromZero) value = 0.f; stage = Attack; }
        void release() { if (stage != Idle) stage = Release; }
        float next()
        {
            switch (stage)
            {
                case Attack:  value += attackInc; if (value >= 1.f) { value = 1.f; stage = Decay; } break;
                case Decay:   value = sustain + (value - sustain) * decCoef;
                              if (std::abs (value - sustain) < 1.0e-4f) { value = sustain; stage = Sustain; } break;
                case Sustain: value = sustain; break;
                case Release: value *= relCoef; if (value < 1.0e-5f) { value = 0.f; stage = Idle; } break;
                case Idle:    value = 0.f; break;
            }
            return value;
        }
    };

    // ------------------------------- TPT state variable filter
    struct Svf
    {
        float ic1 = 0.f, ic2 = 0.f;
        inline void tick (float v0, float a1, float a2, float a3, float& lp, float& bp)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.f * v1 - ic1;
            ic2 = 2.f * v2 - ic2;
            bp = v1; lp = v2;
        }
    };
    struct ChannelFilter { Svf a, b; };

    // ------------------------------- helpers
    static inline float polyBlep (float t, float dt)
    {
        if (t < dt)            { t /= dt;               return t + t - t * t - 1.f; }
        if (t > 1.f - dt)      { t = (t - 1.f) / dt;    return t * t + t + t + 1.f; }
        return 0.f;
    }
    static inline float wrap01 (float x) { return x - std::floor (x); }
    static inline float timeCoef (float ms, double rate)
    {
        const double s = juce::jmax (0.0005, (double) ms * 0.001);
        return (float) std::exp (std::log (0.001) / (s * rate));
    }

    static inline float distort (float x, int type)
    {
        switch (type)
        {
            case 0:  return juce::jlimit (-1.f, 1.f, x);                                  // HARD
            case 1:  return (std::tanh (x + 0.35f) - 0.33638f) * 0.75f;                   // TUBE (asym)
            case 2:  return std::sin (x * 1.5707963f);                                    // FOLD
            default:                                                                      // SCREAM
            {
                float y = std::tanh (x * 1.4f);
                y += 0.55f * (std::abs (y) - 0.5f);                                        // even harmonics
                y = juce::jlimit (-0.92f, 0.92f, y * 1.3f);
                return y;
            }
        }
    }

    static inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 0.8f) return x;
        const float s = x > 0.f ? 1.f : -1.f;
        return s * (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f));
    }

    // ------------------------------- MIDI
    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            const bool wasHeld = ! heldNotes.empty();
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
            heldNotes.push_back (n);
            startNote (n, ! (wasHeld && p.legato));
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            const bool wasCurrent = ! heldNotes.empty() && heldNotes.back() == n;
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
            if (heldNotes.empty())
            {
                gate = false;
                amp.release();
            }
            else if (wasCurrent)
            {
                startNote (heldNotes.back(), ! p.legato);
            }
        }
        else if (m.isPitchWheel())
        {
            pitchWheel = juce::jlimit (-1.f, 1.f, (m.getPitchWheelValue() - 8192) / 8192.f);
        }
        else if (m.isController() && m.getControllerNumber() == 1)
        {
            modWheel = m.getControllerValue() / 127.f;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes.clear(); gate = false; amp.release();
        }
    }

    void startNote (int note, bool retrigger)
    {
        const bool sounding = amp.stage != Env::Idle;
        targetSemi = (float) note;
        if (p.glideMs < 1.f || ! sounding) curSemi = targetSemi;

        if (retrigger || ! gate)
        {
            amp.trigger (! sounding);
            fEnv = 1.f;
            pEnv = 1.f;
            lfoPhase = 0.f;
            shValue = rng.nextFloat() * 2.f - 1.f;
        }
        gate = true;
    }

    // ------------------------------- per-block setup
    void updateBlockTargets()
    {
        const float scr = juce::jlimit (0.f, 1.f, p.scream + modWheel);

        sCutoff.setTargetValue (p.cutoff);
        sReso.setTargetValue (juce::jlimit (0.f, 1.f, p.reso + scr * 0.22f));
        sDrive.setTargetValue (juce::Decibels::decibelsToGain (p.driveDb + scr * 12.f));
        sDMix.setTargetValue (p.dmix);
        sFm.setTargetValue (p.fm + scr * 0.35f);
        sWave.setTargetValue (p.wave);
        sBite.setTargetValue (p.biteHz);

        sVolume.setTargetValue (juce::Decibels::decibelsToGain (p.volumeDb));
        sDelayMix.setTargetValue (p.delayMix);
        sRevMix.setTargetValue (p.revMix);

        const double bpm = juce::jlimit (20.0, 400.0, p.bpm);
        const float dSamples = (float) juce::jlimit (16.0, (double) delaySize - 4.0, p.delayBeats * 60.0 / bpm * sr);
        sDelayTime.setTargetValue (dSamples);

        amp.attackInc = (float) (1.0 / (juce::jmax (0.05, (double) p.attackMs) * 0.001 * osr));
        amp.decCoef   = timeCoef (p.decayMs, osr);
        amp.relCoef   = timeCoef (p.releaseMs, osr);
        amp.sustain   = p.sustain;

        fEnvCoef = timeCoef (p.fdecayMs, osr);
        pEnvCoef = timeCoef (p.pdecayMs, osr);
        glideCoef = p.glideMs < 1.f ? 0.f
                  : (float) std::exp (-1.0 / ((p.glideMs * 0.001 / 3.0) * osr));

        // unison layout
        const int n = juce::jlimit (1, maxUnison, p.unison);
        for (int v = 0; v < n; ++v)
        {
            const float o = n == 1 ? 0.f : -1.f + 2.f * (float) v / (float) (n - 1);
            uniRatio[(size_t) v] = std::pow (2.f, o * p.detuneCents / 1200.f);
            const float pan = o * p.width;
            uniL[(size_t) v] = std::sqrt (0.5f * (1.f - pan));
            uniR[(size_t) v] = std::sqrt (0.5f * (1.f + pan));
        }
        uniCount = n;
        uniNorm = 1.f / std::sqrt ((float) n) * 1.41f;

        juce::Reverb::Parameters rp;
        rp.roomSize = p.revSize; rp.damping = p.revDamp;
        rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
        reverb.setParameters (rp);
    }

    void renderChunked (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        while (len > 0)
        {
            const int n = juce::jmin (len, maxBlockSize);
            renderSegment (buffer, start, n);
            start += n; len -= n;
        }
    }

    // ------------------------------- the actual sound
    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) start, (size_t) len);
        block.clear();

        auto os = oversampler->processSamplesUp (block);
        float* oL = os.getChannelPointer (0);
        float* oR = os.getChannelPointer (1);
        const int osLen = (int) os.getNumSamples();

        const float invOsr = (float) (1.0 / osr);
        const float bpm = (float) juce::jlimit (20.0, 400.0, p.bpm);
        juce::ignoreUnused (bpm);
        const float lfoInc = p.lfoHz * invOsr;
        const float nyq = (float) (osr * 0.45);

        for (int i = 0; i < osLen; ++i)
        {
            // --- modulators
            curSemi = targetSemi + (curSemi - targetSemi) * glideCoef;
            pEnv *= pEnvCoef;
            fEnv *= fEnvCoef;
            const float ampV = amp.next();

            lfoPhase += lfoInc;
            if (lfoPhase >= 1.f) { lfoPhase -= std::floor (lfoPhase); shValue = rng.nextFloat() * 2.f - 1.f; }
            float lfo;
            switch (p.lshape)
            {
                case 0:  lfo = std::sin (lfoPhase * juce::MathConstants<float>::twoPi); break;
                case 1:  lfo = 1.f - 4.f * std::abs (lfoPhase - 0.5f); break;
                case 2:  lfo = 1.f - 2.f * lfoPhase; break;
                case 3:  lfo = lfoPhase < 0.5f ? 1.f : -1.f; break;
                default: lfo = shValue; break;
            }

            const float cutoffBase = sCutoff.getNextValue();
            const float reso  = sReso.getNextValue();
            const float drive = sDrive.getNextValue();
            const float dmix  = sDMix.getNextValue();
            const float fmIdx = sFm.getNextValue() * 6.f;
            const float wave  = sWave.getNextValue();
            const float bite  = sBite.getNextValue();

            if (ampV <= 0.f && amp.stage == Env::Idle)
            {
                oL[i] = 0.f; oR[i] = 0.f;
                continue;
            }

            const float semi = curSemi + pitchWheel * (float) p.bendRange
                             + p.penvSemis * pEnv + p.lpitchSemis * lfo;
            const float baseHz = 440.f * std::pow (2.f, (semi - 69.f) / 12.f);

            // --- unison oscillators
            float l = 0.f, r = 0.f;
            for (int v = 0; v < uniCount; ++v)
            {
                const float hz = juce::jmin (baseHz * uniRatio[(size_t) v], nyq);
                const float dt = hz * invOsr;
                float& ph = carrierPhase[(size_t) v];
                float& mp = modPhase[(size_t) v];
                ph += dt; if (ph >= 1.f) ph -= 1.f;
                mp += dt * p.fmRatio; mp -= std::floor (mp);

                const float pm = fmIdx * std::sin (mp * juce::MathConstants<float>::twoPi) * 0.15915494f;
                const float t = wrap01 (ph + pm);
                const float dte = juce::jlimit (1.0e-5f, 0.5f, dt * (1.f + std::abs (fmIdx) * p.fmRatio));

                const float saw = 2.f * t - 1.f - polyBlep (t, dte);
                float sq = t < 0.5f ? 1.f : -1.f;
                sq += polyBlep (t, dte) - polyBlep (wrap01 (t + 0.5f), dte);

                const float o = saw + wave * (sq - saw);
                l += o * uniL[(size_t) v];
                r += o * uniR[(size_t) v];
            }
            l *= uniNorm * 0.5f;
            r *= uniNorm * 0.5f;

            // --- bite (pre-distortion high-pass)
            const float hpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * bite * invOsr);
            biteState[0] += hpC * (l - biteState[0]); l -= biteState[0];
            biteState[1] += hpC * (r - biteState[1]); r -= biteState[1];

            // --- distortion
            {
                const float dl = distort (l * drive, p.dtype);
                const float dr = distort (r * drive, p.dtype);
                l = l + dmix * (dl - l);
                r = r + dmix * (dr - r);
            }

            // --- filter
            const float modOct = p.fenvOct * fEnv + p.lcutOct * lfo + p.keytrack * (curSemi - 60.f) / 12.f;
            const float fc = juce::jlimit (20.f, nyq * 0.98f, cutoffBase * std::pow (2.f, modOct));
            const float g = std::tan (juce::MathConstants<float>::pi * fc * invOsr);
            const float k = 2.f - 1.96f * reso;
            const float a1 = 1.f / (1.f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;

            l = filterSample (filt[0], l, a1, a2, a3, k);
            r = filterSample (filt[1], r, a1, a2, a3, k);

            oL[i] = l * ampV;
            oR[i] = r * ampV;
        }

        oversampler->processSamplesDown (block);

        // --- base-rate FX: delay -> reverb -> master
        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getWritePointer (1, start);

        const float fbLpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 6500.f / (float) sr);
        const float fbHpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 180.f / (float) sr);
        const float rvHpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 220.f / (float) sr);

        revScratchL.resize ((size_t) len);
        revScratchR.resize ((size_t) len);

        for (int i = 0; i < len; ++i)
        {
            float l = L[i], r = R[i];

            // delay
            const float dTime = sDelayTime.getNextValue();
            const float dMix = sDelayMix.getNextValue();
            float rp = (float) delayWrite - dTime;
            if (rp < 0.f) rp += (float) delaySize;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % delaySize;
            const float fr = rp - (float) i0;
            float dl = delayL[(size_t) i0] + fr * (delayL[(size_t) i1] - delayL[(size_t) i0]);
            float dr = delayR[(size_t) i0] + fr * (delayR[(size_t) i1] - delayR[(size_t) i0]);

            fbLp[0] += fbLpC * (dl - fbLp[0]); dl = fbLp[0];
            fbLp[1] += fbLpC * (dr - fbLp[1]); dr = fbLp[1];
            fbHp[0] += fbHpC * (dl - fbHp[0]); dl -= fbHp[0];
            fbHp[1] += fbHpC * (dr - fbHp[1]); dr -= fbHp[1];

            const float fb = p.delayFb;
            if (p.pingpong)
            {
                delayL[(size_t) delayWrite] = 0.5f * (l + r) + dr * fb;
                delayR[(size_t) delayWrite] = dl * fb;
            }
            else
            {
                delayL[(size_t) delayWrite] = l + dl * fb;
                delayR[(size_t) delayWrite] = r + dr * fb;
            }
            if (++delayWrite >= delaySize) delayWrite = 0;

            l += dl * dMix;
            r += dr * dMix;

            // reverb send (high-passed so the low end stays clean)
            revHp[0] += rvHpC * (l - revHp[0]);
            revHp[1] += rvHpC * (r - revHp[1]);
            revScratchL[(size_t) i] = l - revHp[0];
            revScratchR[(size_t) i] = r - revHp[1];

            L[i] = l; R[i] = r;
        }

        reverb.processStereo (revScratchL.data(), revScratchR.data(), len);

        for (int i = 0; i < len; ++i)
        {
            const float rm = sRevMix.getNextValue();
            const float vol = sVolume.getNextValue();
            const float dryG = 1.f - 0.35f * rm;
            L[i] = softClip ((L[i] * dryG + revScratchL[(size_t) i] * rm) * vol);
            R[i] = softClip ((R[i] * dryG + revScratchR[(size_t) i] * rm) * vol);
        }
    }

    inline float filterSample (ChannelFilter& f, float x, float a1, float a2, float a3, float k)
    {
        float lp, bp;
        f.a.tick (x, a1, a2, a3, lp, bp);
        switch (p.ftype)
        {
            case 0: return lp;                                   // LP 12
            case 1: { float lp2, bp2; f.b.tick (lp, a1, a2, a3, lp2, bp2); return lp2; } // LP 24
            case 2: return bp * (0.6f + k * 0.5f);               // BAND
            default: return x - k * bp - lp;                     // HIGH
        }
    }

    // ------------------------------- state
    EngineParams p;
    double sr = 44100.0, osr = 176400.0;
    int maxBlockSize = 512;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::vector<int> heldNotes;
    bool gate = false;
    float curSemi = 60.f, targetSemi = 60.f, glideCoef = 0.f;
    float pitchWheel = 0.f, modWheel = 0.f;

    Env amp;
    float fEnv = 0.f, fEnvCoef = 0.f, pEnv = 0.f, pEnvCoef = 0.f;
    float lfoPhase = 0.f, shValue = 0.f;
    juce::Random rng;

    std::array<float, maxUnison> carrierPhase { 0.f, 0.31f, 0.67f, 0.13f, 0.89f, 0.45f, 0.58f };
    std::array<float, maxUnison> modPhase {};
    std::array<float, maxUnison> uniRatio {}, uniL {}, uniR {};
    int uniCount = 1;
    float uniNorm = 1.f;

    std::array<float, 2> biteState {};
    std::array<ChannelFilter, 2> filt {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> sCutoff, sBite;
    juce::SmoothedValue<float> sReso, sDrive, sDMix, sFm, sWave;
    juce::SmoothedValue<float> sVolume, sDelayMix, sRevMix, sDelayTime;

    std::vector<float> delayL, delayR;
    int delaySize = 1, delayWrite = 0;
    std::array<float, 2> fbLp {}, fbHp {}, revHp {};

    juce::Reverb reverb;
    std::vector<float> revScratchL, revScratchR;
};
