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
        printf ("%-20s peak %.3f  rms %.3f (%.1f dBFS)  %s\n", proc.getProgramName (prog).toRawUTF8(), peak, rms,
                Decibels::gainToDecibels ((float) rms), pass ? "OK" : (nan ? "NAN!" : "FAIL"));
    }

    auto setP = [&proc] (const String& id, float v)
    {
        if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    auto renderRiff = [&] (int bars) -> float   // returns peak, -1 on NaN
    {
        float pk = 0.f;
        for (int pos = 0; pos < barSamples * bars; pos += block)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
            for (auto& n : riff)
            {
                const int on = n.startEighth * beat / 2, off = (n.startEighth + n.lenEighth) * beat / 2 + beat / 16;
                if (on >= pos && on < pos + block)   midi.addEvent (MidiMessage::noteOn (1, n.note, (uint8) 110), on - pos);
                if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, n.note), off - pos);
            }
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) return -1.f;
                    pk = jmax (pk, std::abs (v));
                }
        }
        // release everything and let it ring out
        for (int b = 0; b < 400; ++b)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer m;
            if (b == 0) m.addEvent (MidiMessage::allNotesOff (1), 0);
            proc.processBlock (buf, m);
        }
        return pk;
    };

    // ---- v1 compatibility: the v1 presets must sound the same as in v1.0
    if (argc > 2)
    {
        AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> rd (fm.createReaderFor (File (argv[2])));
        if (rd != nullptr)
        {
            AudioBuffer<float> v1 (2, (int) rd->lengthInSamples);
            rd->read (&v1, 0, (int) rd->lengthInSamples, 0, true, true);
            const int seg = barSamples * 2;
            const int nV1 = v1.getNumSamples() / seg;
            for (int prog = 0; prog < nV1; ++prog)
            {
                double e = 0.0, ref = 0.0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < seg; ++i)
                    {
                        const float a = v1.getSample (ch, prog * seg + i), b = all.getSample (ch, prog * seg + i);
                        e += (a - b) * (a - b); ref += a * a;
                    }
                const double rel = std::sqrt (e / jmax (1e-12, ref));
                const String name = proc.getProgramName (prog);
                const bool random = name == "Frenchcore Rip";     // S&H LFO is random by design
                const bool pass = random || rel < 0.02;
                ok &= pass;
                printf ("v1 match %-18s diff %.4f %s\n", name.toRawUTF8(), rel, random ? "(S&H, skipped)" : (pass ? "OK" : "FAIL"));
            }
        }
        else { printf ("v1 reference not readable\n"); ok = false; }
    }

    // ---- states saved by v1.0 (no OSC2 / FX switches) load with OSC2 off and all FX on
    {
        proc.setCurrentProgram (0);
        MemoryBlock mb; proc.getStateInformation (mb);
        auto xml = AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        for (int i = xml->getNumChildElements(); --i >= 0;)
        {
            const auto id = xml->getChildElement (i)->getStringAttribute ("id");
            if (id.startsWith ("o1_") || id.startsWith ("o2_") || id == "dly_on" || id == "rev_on")
                xml->removeChildElement (xml->getChildElement (i), true);
        }
        MemoryBlock old; AudioProcessor::copyXmlToBinary (*xml, old);
        setP ("o2_on", 1); setP ("o1_dist_on", 0); setP ("rev_on", 0); setP ("o1_on", 0);
        proc.setStateInformation (old.getData(), (int) old.getSize());
        auto v = [&proc] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const bool compat = v ("o2_on") < 0.5f && v ("o1_on") > 0.5f && v ("o1_dist_on") > 0.5f && v ("rev_on") > 0.5f;
        printf ("v1 project state -> osc1 on, osc2 off, fx on: %s\n", compat ? "OK" : "FAIL");
        ok &= compat;
    }

    // ---- OSC2 alone, both off, FX switches
    {
        proc.setCurrentProgram (0);
        setP ("o1_on", 0); setP ("o2_on", 1);
        const float pk2 = renderRiff (1);
        printf ("osc2 only -> peak %.3f %s\n", pk2, pk2 > 0.05f ? "OK" : "FAIL");
        ok &= pk2 > 0.05f;

        setP ("o2_on", 0); setP ("dly_on", 0); setP ("rev_on", 0);
        const float pk0 = renderRiff (1);
        printf ("both oscillators off -> peak %.5f %s\n", pk0, pk0 >= 0.f && pk0 < 1e-4f ? "OK" : "FAIL");
        ok &= pk0 >= 0.f && pk0 < 1e-4f;

        proc.setCurrentProgram (0);
        const char* sw[] = { "o1_dist_on", "o1_filt_on", "o1_lfo_on", "dly_on", "rev_on" };
        for (auto* id : sw)
        {
            setP (id, 0);
            const float pk = renderRiff (1);
            printf ("%-12s off -> peak %.3f %s\n", id, pk, pk > 0.02f ? "OK" : "FAIL");
            ok &= pk > 0.02f;
            setP (id, 1);
        }
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

    // randomize + user preset save/load round-trip
    for (int i = 0; i < 20; ++i)
    {
        proc.randomize();
        AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
        midi.addEvent (MidiMessage::noteOn (1, 60, (uint8) 110), 0);
        float pk = 0.f; bool bad = false;
        for (int b = 0; b < 60; ++b)
        {
            buf.clear(); proc.processBlock (buf, midi); midi.clear();
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < block; ++s) { auto v = buf.getSample (ch, s); bad |= ! std::isfinite (v); pk = jmax (pk, std::abs (v)); }
        }
        MidiBuffer off; off.addEvent (MidiMessage::allNotesOff (1), 0); buf.clear(); proc.processBlock (buf, off);
        if (bad || pk < 0.02f) { printf ("randomize #%d FAIL (peak %.3f)\n", i, pk); ok = false; }
    }
    printf ("randomize x20: done\n");
    {
        const float cut = proc.apvts.getRawParameterValue ("cutoff")->load();
        const bool saved = proc.saveUserPreset ("__test_preset");
        auto f = ProphecyProcessor::getUserPresetFolder().getChildFile ("__test_preset.pos");
        proc.setCurrentProgram (0);
        const bool loaded = proc.loadUserPreset (f);
        const bool same = std::abs (proc.apvts.getRawParameterValue ("cutoff")->load() - cut) < 1.f;
        printf ("user preset save/load: %s\n", (saved && loaded && same) ? "OK" : "FAIL");
        ok &= saved && loaded && same;
        f.deleteFile();
    }

    // write wav
    {
        auto f = outDir.getChildFile ("prophecy_demo.wav");
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }

    // GUI snapshot
    proc.setCurrentProgram (proc.getNumPrograms() - 1);   // Twin Prophecy (both oscillators)
    for (int b = 0; b < 40; ++b)
    {
        AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
        if (b == 0) midi.addEvent (MidiMessage::noteOn (1, 62, (uint8) 110), 0);
        proc.processBlock (buf, midi);
    }
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1360, 800);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("prophecy_gui.png");
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);

        // second shot: FILTER tab on OSC 1, LFO tab on OSC 2
        std::function<void (Component&, int&)> walk = [&walk] (Component& c, int& n)
        {
            if (auto* t = dynamic_cast<FxTabs*> (&c)) t->selectTab (n++ == 0 ? 1 : 2);
            for (auto* ch : c.getChildren()) walk (*ch, n);
        };
        int n = 0; walk (*ed, n);
        auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f2 = outDir.getChildFile ("prophecy_gui_tabs.png");
        f2.deleteFile();
        FileOutputStream os2 (f2);
        PNGImageFormat().writeImageToStream (img2, os2);
    }

    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
