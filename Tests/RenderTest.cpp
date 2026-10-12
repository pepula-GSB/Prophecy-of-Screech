// Offline render test: plays a hardstyle screech riff through every preset,
// writes a WAV, checks for NaN / silence / clipping and snapshots the GUI.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();

    const double sr = 48000.0;
    const int block = 512;
    ProphecyProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    // riff @150 BPM: 1/8 notes, legato slides (beat = 19200 samples)
    const int beat = (int) (sr * 60.0 / 150.0);
    struct N { int startEighth, lenEighth, note; };
    const std::vector<N> riff { { 0, 2, 62 }, { 2, 1, 65 }, { 3, 2, 69 }, { 4, 1, 67 }, { 5, 3, 74 } };
    const int barSamples = beat * 4;

    AudioBuffer<float> all (2, barSamples * 2 * proc.getNumPrograms());
    all.clear();
    bool ok = true;

    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        double sumSq = 0.0; float peak = 0.f; bool nan = false;

        for (int pos = 0; pos < barSamples * 2; pos += block)
        {
            AudioBuffer<float> buf (2, block);
            buf.clear();
            MidiBuffer midi;
            for (auto& n : riff)
            {
                const int on = n.startEighth * beat / 2, off = (n.startEighth + n.lenEighth) * beat / 2 + beat / 16;
                if (on >= pos && on < pos + block)   midi.addEvent (MidiMessage::noteOn (1, n.note, (uint8) 110), on - pos);
                if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, n.note), off - pos);
            }
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* d = buf.getReadPointer (ch);
                for (int i = 0; i < block; ++i)
                {
                    if (! std::isfinite (d[i])) nan = true;
                    peak = jmax (peak, std::abs (d[i]));
                    sumSq += d[i] * d[i];
                }
                const int dst = prog * barSamples * 2 + pos;
                all.copyFrom (ch, dst, buf, ch, 0, jmin (block, all.getNumSamples() - dst));
            }
        }
        const double rms = std::sqrt (sumSq / (barSamples * 4.0));
        const bool pass = ! nan && peak > 0.05f && peak <= 1.0f;
        ok &= pass;
        printf ("%-18s peak %.3f  rms %.3f (%.1f dBFS)  %s\n", proc.getProgramName (prog).toRawUTF8(), peak, rms,
                Decibels::gainToDecibels ((float) rms), pass ? "OK" : (nan ? "NAN!" : "FAIL"));
    }

    // state round-trip
    MemoryBlock state;
    proc.setCurrentProgram (3);
    proc.getStateInformation (state);
    proc.setCurrentProgram (0);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    const bool stateOk = std::abs (proc.apvts.getRawParameterValue ("glide")->load() - 400.f) < 0.5f;
    printf ("state round-trip: %s\n", stateOk ? "OK" : "FAIL");
    ok &= stateOk;

    // write wav
    {
        auto f = outDir.getChildFile ("prophecy_demo.wav");
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }

    // GUI snapshot
    proc.setCurrentProgram (1);
    for (int b = 0; b < 40; ++b)
    {
        AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
        if (b == 0) midi.addEvent (MidiMessage::noteOn (1, 62, (uint8) 110), 0);
        proc.processBlock (buf, midi);
    }
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1280, 760);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("prophecy_gui.png");
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
    }

    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
