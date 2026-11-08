// Checks the chord presets, every sound, extensions, and chord pulling from MIDI and audio.
#include "../Source/PluginProcessor.h"
#include "../Source/ChordExtractor.h"
#include "../Source/PresetFile.h"
#include <thread>

namespace
{
    int failures = 0;
    // Lets the plugin's background timers run for a moment (parameters reach the undo history this way)
    void pump (int ms)
    {
        std::thread t ([ms] { juce::Thread::sleep (ms); juce::MessageManager::callAsync ([] { juce::MessageManager::getInstance()->stopDispatchLoop(); }); });
        juce::MessageManager::getInstance()->runDispatchLoop();
        t.join();
    }
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
            const auto c = p.chordForKey (pc, 0);
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
        const auto c = p.chordForKey (0, 0);
        const bool ok = (ext == 1 && (c.name.endsWith ("7") || c.name.contains ("7("))) || (ext == 2 && c.name.contains ("9")) || (ext == 3 && c.name.contains ("11"));
        check (ok, juce::String ("extensions ") + label, c.name);
    }
    set (p, "extensions", 0.0f);

    // 3. Every sound renders without blowing up or going silent
    set (p, "chordSet", 0.0f);
    for (size_t s = 0; s < soundPresets().size(); ++s)
    {
        p.applySoundPreset (0, (int) s);
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
        p.setUserChords (0, ChordExtractor::fromMidi (mf, "save test").set);
        juce::MemoryBlock state;
        p.getStateInformation (state);
        SoulKeysProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (q.hasUserChords (0) && q.chordForKey (0, 0).name == p.chordForKey (0, 0).name, "file chords saved with the project", q.chordForKey (0, 0).name);
    }

    // 7. Key: move preset 1 (written in Eb) to C
    {
        SoulKeysProcessor k;
        set (k, "chordSet", 0.0f);
        set (k, "key", 1.0f); // index 1 = C
        check (k.chordForKey (0, 0).name == "Cmaj9", "key change moves the chord set", k.chordForKey (0, 0).name);
    }

    // 8. Key & Scale chords
    {
        SoulKeysProcessor k;
        set (k, "chordSet", (float) SoulKeysProcessor::scaleSlot);
        set (k, "key", 1.0f);   // C
        set (k, "scale", 0.0f); // Major
        juce::StringArray major;
        for (int pc : ChordPresets::whitePcs) major.add (k.chordForKey (pc, 0).name);
        check (major.joinIntoString (" ") == "Cmaj9 Dm9 Em7 Fmaj9 G9 Am9 Bm7(b5)", "C major scale chords", major.joinIntoString (" "));
        set (k, "key", 10.0f);  // A
        set (k, "scale", 1.0f); // Minor
        juce::StringArray minor;
        for (int pc : ChordPresets::whitePcs) minor.add (k.chordForKey (pc, 0).name);
        check (minor[0] == "Am9" && minor[3] == "Dm9", "A minor scale chords", minor.joinIntoString (" "));
        set (k, "extensions", 3.0f);
        check (k.chordForKey (0, 0).name == "Am11", "scale chords follow the 11th switch", k.chordForKey (0, 0).name);
    }

    // 9. Split mode: low keys use zone A, high keys zone B, same register, and both sound
    {
        SoulKeysProcessor k;
        k.setRateAndBufferSizeDetails (48000.0, 512);
        k.prepareToPlay (48000.0, 512);
        set (k, "split", 1.0f);
        set (k, "splitPoint", 60.0f);
        set (k, "chordSet", 0.0f);
        set (k, "chordSetB", 1.0f);
        check (k.zoneOf (48) == 0 && k.zoneOf (60) == 1 && k.zoneOf (72) == 1, "split point sends keys to the right zone");
        const auto a = k.chordForKey (0, 0), b = k.chordForKey (0, 1);
        check (a.name != b.name, "zones play different chord sets", a.name + " / " + b.name);
        k.applySoundPreset (1, 7); // zone B: tape piano
        bool finite = true;
        const float peakA = play (k, 48, 0.6f, finite);
        const float peakB = play (k, 72, 0.6f, finite);
        check (finite && peakA > 0.02f && peakB > 0.02f, "both zones make sound", juce::String (peakA, 3) + " / " + juce::String (peakB, 3));
        const auto before = k.chordForKey (0, 0).name;
        k.swapZones();
        check (k.chordForKey (0, 1).name == before, "swap A / B", k.chordForKey (0, 1).name);
        set (k, "split", 0.0f);
        check (k.zoneOf (72) == 0, "split off: every key uses zone A");
    }

    // 10. Chord names typed by hand
    {
        juce::StringArray got;
        for (auto n : { "Ebmaj9", "F#m11", "C13(b9)", "Bbm7b5", "G7sus", "Db6/9", "Am", "E7#9", "Gbmaj9#11", "Bb13sus4", "Fdim7", "C/E" })
            got.add (Voicing::parseName (n).name);
        check (got.joinIntoString (" ") == "Ebmaj9 Gbm11 C13(b9) Bbm7(b5) G7sus4 Db6/9 Am7 E7(#9) Gbmaj9(#11) Bb13sus4 Fdim7 Cadd9", "read chord names", got.joinIntoString (" "));
    }

    // 11. Bonus presets import, and a saved preset comes back identical
    {
        const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile ("Bonus Presets");
        int ok = 0;
        juce::String errors;
        for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.soulkeys"))
        {
            ChordSet set; juce::String error;
            if (PresetFile::load (f, set, error)) ++ok; else errors << f.getFileName() << ": " << error << " ";
        }
        check (ok == 3, "bonus presets load", errors);

        SoulKeysProcessor k;
        const auto before = k.numChordSets();
        const auto importError = k.importPreset (folder.getChildFile ("Philly Rooftop.soulkeys"), 0);
        check (importError.isEmpty() && k.chordForKey (0, 0).name == "Dmaj9" && k.chordForKey (1, 0).name == "B13(b9)",
               "import a preset and play it", importError + " " + k.chordForKey (0, 0).name);

        set (k, "chordSet", 3.0f);
        set (k, "key", 1.0f); // move to C before saving
        const auto saveError = k.saveZoneAsPreset (0, "Smoke Test Preset");
        juce::StringArray names;
        for (int pc = 0; pc < 12; ++pc) names.add (k.chordForKey (pc, 0).name);
        check (saveError.isEmpty() && k.numChordSets() > before && k.chordSetName (0, juce::roundToInt (k.apvts.getRawParameterValue ("chordSet")->load())) == "My: Smoke Test Preset",
               "save chords as a new preset", saveError);
        ChordSet reread; juce::String error;
        PresetFile::load (PresetFile::folder().getChildFile ("Smoke Test Preset.soulkeys"), reread, error);
        juce::StringArray rereadNames;
        for (auto& c : reread.chords) rereadNames.add (c.name);
        check (rereadNames == names, "saved preset reads back the same", rereadNames[0]);
        PresetFile::folder().getChildFile ("Smoke Test Preset.soulkeys").deleteFile();
        PresetFile::folder().getChildFile ("Philly Rooftop.soulkeys").deleteFile();
    }

    // 12. Recorder captures the chords that were played
    {
        SoulKeysProcessor k;
        k.setRateAndBufferSizeDetails (48000.0, 512);
        k.prepareToPlay (48000.0, 512);
        k.startRecording();
        bool finite = true;
        play (k, 60, 0.5f, finite);
        play (k, 62, 0.5f, finite);
        k.stopRecording();
        std::vector<SoulKeysProcessor::RecEvent> take;
        k.drainRecording (take);
        int ons = 0, offs = 0;
        for (auto& e : take) (e.on ? ons : offs)++;
        const int expected = (int) (k.chordForKey (0, 0).notes.size() + k.chordForKey (2, 0).notes.size());
        check (ons == expected && offs == expected, "recorder captures every chord note", juce::String (ons) + " on / " + juce::String (offs) + " off");
    }

    // 13. Voicing styles and top note
    {
        SoulKeysProcessor k;
        auto span = [] (const Chord& c) { return c.notes.back() - c.notes.front(); };
        auto upperSpan = [] (const Chord& c) { std::vector<int> u; for (int n : c.notes) if (n >= 48) u.push_back (n); return u.empty() ? 0 : u.back() - u.front(); };
        const auto closeChord = k.chordForKey (2, 0); // Fm9
        set (k, "voicing", 2.0f);
        const auto drop2 = k.chordForKey (2, 0);
        set (k, "voicing", 1.0f);
        const auto open = k.chordForKey (2, 0);
        check (upperSpan (drop2) > upperSpan (closeChord) && span (open) >= span (drop2), "drop 2 and open spread the chord",
               juce::String (upperSpan (closeChord)) + " / " + juce::String (upperSpan (drop2)) + " / " + juce::String (span (open)));
        set (k, "voicing", 3.0f);
        const auto rootless = k.chordForKey (2, 0);
        const bool noRoot = std::none_of (rootless.notes.begin(), rootless.notes.end(), [] (int n) { return n % 12 == 5; }); // F
        check (noRoot && rootless.notes.front() >= 48, "rootless leaves out the root and bass", rootless.name);
        set (k, "voicing", 0.0f);
        bool topOk = true;
        juce::String tops;
        for (auto [degree, interval] : { std::pair { 2, 3 }, { 4, 10 }, { 5, 14 }, { 1, 0 } }) // Fm9: 3rd = Ab, 7th = Eb, 9th = G, root = F
        {
            set (k, "topNote", (float) degree);
            const auto c = k.chordForKey (2, 0);
            topOk = topOk && c.notes.back() % 12 == (5 + interval) % 12;
            tops << ChordQ::noteName (c.notes.back()) << " ";
        }
        check (topOk, "top note puts the chosen chord tone on top (Ab, Eb, G, F)", tops);
    }

    // 14. MIDI output
    {
        SoulKeysProcessor k;
        k.setRateAndBufferSizeDetails (48000.0, 512);
        k.prepareToPlay (48000.0, 512);
        set (k, "midiOut", 2.0f);  // MIDI only
        set (k, "strum", 0.0f);
        set (k, "humanize", 0.0f);
        set (k, "split", 1.0f);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 50, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 72, 0.8f), 0);
        k.processBlock (buf, midi);
        int ch1 = 0, ch2 = 0;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn()) (m.getChannel() == 1 ? ch1 : ch2)++;
        }
        const int expectA = (int) k.chordForKey (2, 0).notes.size(), expectB = (int) k.chordForKey (0, 1).notes.size();
        check (ch1 == expectA && ch2 == expectB && buf.getMagnitude (0, 512) < 1.0e-6f, "MIDI only: chords go out as MIDI, zone A ch 1, zone B ch 2, no sound",
               juce::String (ch1) + "+" + juce::String (ch2) + " notes");
        set (k, "midiOut", 0.0f);
        juce::MidiBuffer midi2;
        midi2.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);
        k.processBlock (buf, midi2);
        check (midi2.isEmpty(), "MIDI output off sends nothing");
    }

    // 14b. Silence when nothing plays (no tape hiss leaking from an unused zone)
    {
        SoulKeysProcessor k;
        k.setRateAndBufferSizeDetails (48000.0, 512);
        k.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        float peak = 0.0f;
        for (int b = 0; b < 300; ++b) { juce::MidiBuffer none; k.processBlock (buf, none); if (b > 20) peak = std::max (peak, buf.getMagnitude (0, 512)); }
        check (peak < 1.0e-4f, "silent when nothing is playing", juce::String (peak, 6));
    }

    // 15. Undo and compare
    {
        SoulKeysProcessor k;
        set (k, "chordSet", 7.0f);
        (void) k.apvts.copyState(); // copies parameter changes into the state, as the plugin's timer does
        k.undoManager.beginNewTransaction();
        set (k, "chordSet", 20.0f);
        (void) k.apvts.copyState(); // copies parameter changes into the state, as the plugin's timer does
        k.undoManager.beginNewTransaction();
        k.undoManager.undo();
        (void) k.apvts.copyState(); // copies parameter changes into the state, as the plugin's timer does
        check (juce::roundToInt (k.apvts.getRawParameterValue ("chordSet")->load()) == 7, "undo restores the previous setting");
        k.undoManager.redo();
        (void) k.apvts.copyState(); // copies parameter changes into the state, as the plugin's timer does
        check (juce::roundToInt (k.apvts.getRawParameterValue ("chordSet")->load()) == 20, "redo");

        k.switchCompare (1);
        set (k, "chordSet", 33.0f);
        k.switchCompare (0);
        const int one = juce::roundToInt (k.apvts.getRawParameterValue ("chordSet")->load());
        k.switchCompare (1);
        const int two = juce::roundToInt (k.apvts.getRawParameterValue ("chordSet")->load());
        check (one == 20 && two == 33, "compare 1 / 2 keeps two setups", juce::String (one) + " / " + juce::String (two));
    }

    // 16. Next-chord suggestions: after G7 the C chord should come first
    {
        std::array<Chord, 12> options;
        const char* names[] { "Cmaj9", "Db9", "Dm9", "Eb13", "Em7", "Fmaj9", "F#m7b5", "G13", "Abmaj7", "Am9", "Bb9", "Bm7b5" };
        for (int i = 0; i < 12; ++i) options[(size_t) i] = Voicing::parseName (names[i]);
        const auto scores = Suggest::score (Voicing::parseName ("G7"), options);
        const int best = (int) (std::max_element (scores.begin(), scores.end()) - scores.begin());
        check (best == 0, "suggestion after G7 is Cmaj9", options[(size_t) best].name);
    }

    std::printf (failures == 0 ? "PASS\n" : "FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
