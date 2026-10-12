#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    // OSC
    inline constexpr const char* wave      = "wave";
    inline constexpr const char* unison    = "unison";
    inline constexpr const char* detune    = "detune";
    inline constexpr const char* width     = "width";
    inline constexpr const char* fm        = "fm";
    inline constexpr const char* fmRatio   = "fmratio";
    // PITCH
    inline constexpr const char* glide     = "glide";
    inline constexpr const char* penv      = "penv";
    inline constexpr const char* pdecay    = "pdecay";
    inline constexpr const char* bend      = "bend";
    inline constexpr const char* legato    = "legato";
    // MACRO
    inline constexpr const char* scream    = "scream";
    // FILTER
    inline constexpr const char* ftype     = "ftype";
    inline constexpr const char* cutoff    = "cutoff";
    inline constexpr const char* reso      = "reso";
    inline constexpr const char* fenv      = "fenv";
    inline constexpr const char* fdecay    = "fdecay";
    inline constexpr const char* keytrack  = "keytrack";
    // DIST
    inline constexpr const char* dtype     = "dtype";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* bite      = "bite";
    inline constexpr const char* dmix      = "dmix";
    // LFO
    inline constexpr const char* lshape    = "lshape";
    inline constexpr const char* lsync     = "lsync";
    inline constexpr const char* lrate     = "lrate";
    inline constexpr const char* lfoDiv      = "lfoDiv";
    inline constexpr const char* lcut      = "lcut";
    inline constexpr const char* lpitch    = "lpitch";
    // AMP
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* decay     = "decay";
    inline constexpr const char* sustain   = "sustain";
    inline constexpr const char* release   = "release";
    // DELAY
    inline constexpr const char* dtime     = "dtime";
    inline constexpr const char* dfb       = "dfb";
    inline constexpr const char* dfx       = "dfx";
    inline constexpr const char* pingpong  = "pingpong";
    // REVERB
    inline constexpr const char* rsize     = "rsize";
    inline constexpr const char* rdamp     = "rdamp";
    inline constexpr const char* rmix      = "rmix";
    // MASTER
    inline constexpr const char* volume    = "volume";
}

namespace ParamChoices
{
    inline const juce::StringArray fmRatios   { "0.5", "1", "1.5", "2", "3", "4", "5", "7" };
    inline const float fmRatioValues[]        { 0.5f, 1.f, 1.5f, 2.f, 3.f, 4.f, 5.f, 7.f };

    inline const juce::StringArray filterTypes { "LP 12", "LP 24", "BAND", "HIGH" };
    inline const juce::StringArray distTypes   { "HARD", "TUBE", "FOLD", "SCREAM" };
    inline const juce::StringArray lfoShapes   { "SINE", "TRI", "SAW", "SQUARE", "S&H" };

    inline const juce::StringArray divisions   { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                                 "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1" };
    inline const float divisionBeats[]         { 0.125f, 1.f / 6.f, 0.25f, 0.375f, 1.f / 3.f, 0.5f, 0.75f,
                                                 2.f / 3.f, 1.f, 1.5f, 2.f, 4.f, 8.f };
}
