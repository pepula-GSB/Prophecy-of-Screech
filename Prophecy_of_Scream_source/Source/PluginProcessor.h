#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ScreechEngine.h"
#include "Params.h"
#include <array>
#include <atomic>

class ProphecyProcessor : public juce::AudioProcessor
{
public:
    ProphecyProcessor();
    ~ProphecyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Prophecy of Scream"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // oscilloscope feed (written on audio thread, read on UI thread)
    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scopeBuffer {};
    std::atomic<int> scopeWritePos { 0 };

private:
    EngineParams gatherParams (double bpm) const;

    struct Ptrs
    {
        std::atomic<float> *wave, *unison, *detune, *width, *fm, *fmRatio,
                           *glide, *penv, *pdecay, *bend, *legato, *scream,
                           *ftype, *cutoff, *reso, *fenv, *fdecay, *keytrack,
                           *dtype, *drive, *bite, *dmix,
                           *lshape, *lsync, *lrate, *lfoDiv, *lcut, *lpitch,
                           *attack, *decay, *sustain, *release,
                           *dtime, *dfb, *dfx, *pingpong, *rsize, *rdamp, *rmix, *volume;
    } ptr {};
    ScreechEngine engine;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProphecyProcessor)
};
