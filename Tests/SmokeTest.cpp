// Checks the chord presets, every sound, extensions, and chord pulling from MIDI and audio.
#include "../Source/PluginProcessor.h"
#include "../Source/ChordExtractor.h"

namespace
{
    int failures = 0;
    void check (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        std::printf ("%s %s %s\n", ok ? "  ok  " : "  FAIL", what.toRawUTF8(), detail.toRawUTF8());
        if (! ok) ++failures;
    }
    void set (SoulKeysProcessor& p, const char* id, float v)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }
    // Plays one key for `seconds`; returns peak level
    float play (SoulKeysProcessor& p, int key, float seconds, bool& finite)
    {
        juce::AudioBuffer<float> buf (2, 512);
        float peak = 0.0f;
        const int blocks = (int) (seconds * 48000.0 / 512.0);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);
            if (b == blocks - 1) midi.addEvent (juce::MidiMessage::noteOff (1, key), 0);
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    finite = finite && std::isfinite (v);
                    peak = std::max (peak, std::abs (v));
                }
        }
        return peak;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    SoulKeysProcessor p;
    p.setRateAndBufferSizeDetails (48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    // 1. Every preset gives 12 playable chords
    int badChords = 0, total = 0;
    juce::StringArray sample;
    for (int s = 0; s < ChordPresets::count(); ++s)
    {
        set (p, "chordSet", (float) s);
        for (int pc = 0; pc < 12; ++pc)
        {
            const auto c = p.chordForKey (pc);
            ++total;
            const bool ok = c.notes.size() >= 4 && c.name.isNotEmpty()
                         && std::all_of (c.notes.begin(), c.notes.end(), [] (int n) { return n >= 24 && n <= 100; });
            if (! ok) ++badChords;
            if (s == 0) sample.add (ChordQ::noteName (pc) + "=" + c.name);
        }
    }
    check (ChordPresets::count() == 50, "50 chord presets", juce::String (ChordPresets::count()));
    check (badChords == 0, "every key of every preset plays a full chord", juce::String (total - badChords) + "/" + juce::String (total));
    std::printf ("        preset 1: %s\n", sample.joinIntoString (", ").toRawUTF8());

    // 2. Extensions switch
    set (p, "chordSet", 1.0f);
    for (auto [ext, label] : { std::pair { 1, "7th" }, { 2, "9th" }, { 3, "11th" } })
    {
        set (p, "extensions", (float) ext);
        const auto c = p.chordForKey (0);
        const bool ok = (ext == 1 && (c.name.endsWith ("7") || c.name.contains ("7("))) || (ext == 2 && c.name.contains ("9")) || (ext == 3 && c.name.contains ("11"));
        check (ok, juce::String ("extensions ") + label, c.name);
    }
    set (p, "extensions", 0.0f);

    // 3. Every sound renders without blowing up or going silent
    set (p, "chordSet", 0.0f);
    for (size_t s = 0; s < soundPresets().size(); ++s)
    {
        p.applySoundPreset ((int) s);
        bool finite = true;
        const float peak = play (p, 60 + (int) s % 12, 1.0f, finite);
        play (p, 0, 0.6f, finite); // let it ring out
        check (finite && peak > 0.02f && peak < 3.0f, juce::String ("sound: ") + soundPresets()[s].name, "peak " + juce::String (peak, 3));
    }

    // 4. Chords from a MIDI file
    {
        juce::MidiMessageSequence seq;
        const std::vector<std::vector<int>> chords { { 48, 52, 55, 59 }, { 45, 52, 55, 60, 59 }, { 41, 53, 57, 60, 64 }, { 43, 53, 59, 62 } };
        double t = 0.0;
        for (auto& c : chords)
        {
            for (int n : c) { seq.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), t * 960); seq.addEvent (juce::MidiMessage::noteOff (1, n), (t + 1.9) * 960); }
            t += 2.0;
        }
        seq.addEvent (juce::MidiMessage::noteOn (10, 36, 1.0f), 0); // a drum hit to ignore
        seq.updateMatchedPairs();
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (960);
        mf.addTrack (seq);
        auto r = ChordExtractor::fromMidi (mf, "test");
        juce::StringArray got;
        for (int i = 0; i < r.chordsFound; ++i) got.add (r.set.chords[(size_t) ChordPresets::whitePcs[(size_t) i]].name);
        check (r.chordsFound == 4 && got[0] == "Cmaj7" && got[3].startsWith ("G7"), "chords from MIDI", got.joinIntoString (", "));
    }

    // 5. Chords from audio (synthesised C, Am, F, G7 played two seconds each)
    {
        const double sr = 44100.0;
        const std::vector<std::vector<int>> chords { { 48, 60, 64, 67 }, { 45, 57, 60, 64 }, { 41, 53, 57, 60 }, { 43, 55, 59, 62, 65 } };
        juce::AudioBuffer<float> audio (1, (int) (sr * 8.0));
        audio.clear();
        for (size_t c = 0; c < chords.size(); ++c)
            for (int n : chords[c])
            {
                const double f = juce::MidiMessage::getMidiNoteInHertz (n);
                for (int i = 0; i < (int) (sr * 2.0); ++i)
                {
                    const double t = i / sr;
                    const float env = (float) std::exp (-t * 0.8);
                    float s = 0.0f;
                    for (int h = 1; h <= 4; ++h) s += (float) std::sin (juce::MathConstants<double>::twoPi * f * h * t) / (float) (h * h);
                    audio.addSample (0, (int) c * (int) (sr * 2.0) + i, 0.08f * env * s);
                }
            }
        auto r = ChordExtractor::fromAudio (audio, sr, "test");
        juce::StringArray got;
        std::vector<int> roots;
        for (int i = 0; i < std::min (r.chordsFound, 7); ++i)
        {
            const auto& c = r.set.chords[(size_t) ChordPresets::whitePcs[(size_t) i]];
            got.add (c.name);
            roots.push_back (c.root);
        }
        const bool ok = roots.size() >= 4 && roots[0] == 0 && roots[1] == 9 && roots[2] == 5 && roots[3] == 7;
        check (ok, "chords from audio (expect C, A, F, G roots)", got.joinIntoString (", "));
    }

    // 6. Chords pulled from a file survive save and reload
    {
        juce::MidiMessageSequence seq;
        for (int n : { 50, 53, 57, 60 }) { seq.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0); seq.addEvent (juce::MidiMessage::noteOff (1, n), 1900); }
        seq.updateMatchedPairs();
        juce::MidiFile mf; mf.setTicksPerQuarterNote (960); mf.addTrack (seq);
        p.setUserChords (ChordExtractor::fromMidi (mf, "save test").set);
        juce::MemoryBlock state;
        p.getStateInformation (state);
        SoulKeysProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (q.hasUserChords() && q.chordForKey (0).name == p.chordForKey (0).name, "file chords saved with the project", q.chordForKey (0).name);
    }

    std::printf (failures == 0 ? "PASS\n" : "FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
