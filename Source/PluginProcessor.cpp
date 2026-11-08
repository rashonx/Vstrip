#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetFile.h"

namespace
{
    std::vector<ChordSet>& presetSets()
    {
        static std::vector<ChordSet> sets = []
        {
            std::vector<ChordSet> s;
            for (int i = 0; i < ChordPresets::count(); ++i)
                s.push_back (ChordPresets::get (i));
            return s;
        }();
        return sets;
    }

    juce::StringArray keyChoices()
    {
        juce::StringArray k { "Original" };
        for (int pc = 0; pc < 12; ++pc) k.add (ChordQ::noteName (pc));
        return k;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SoulKeysProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> fmt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    StringArray sounds;
    for (auto& s : soundPresets()) sounds.add (s.name);

    for (int z = 0; z < numZones; ++z)
    {
        const String zn = z == 0 ? " A" : " B";
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid (z, "chordSet"), 1 }, "Chord Set" + zn, 0, libraryStart + maxLibrary - 1, z == 0 ? 0 : 1));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (z, "extensions"), 1 }, "Extensions" + zn, StringArray { "As preset", "7th", "9th", "11th" }, 0));
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid (z, "octave"), 1 }, "Octave" + zn, -2, 2, 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (z, "key"), 1 }, "Key" + zn, keyChoices(), 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (z, "scale"), 1 }, "Scale" + zn, ScaleChords::scaleNames(), 0));
        const int soundDefault = z == 0 ? 0 : 6;
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (z, "sound"), 1 }, "Sound" + zn, sounds, soundDefault));
        const auto& s = soundPresets()[(size_t) soundDefault];
        addFloat (pid (z, "bell"), "Bell" + zn, { 0.0f, 1.0f }, s.bell, pct);
        addFloat (pid (z, "bark"), "Bark" + zn, { 0.0f, 1.0f }, s.bark, pct);
        addFloat (pid (z, "decay"), "Decay" + zn, { 0.5f, 2.0f }, s.decay, [] (float v, int) { return String (v, 2) + "x"; });
        addFloat (pid (z, "tone"), "Tone" + zn, { 0.0f, 1.0f }, s.tone, pct);
        addFloat (pid (z, "tremRate"), "Tremolo Rate" + zn, { 0.5f, 10.0f }, s.tremRate, [] (float v, int) { return String (v, 1) + " Hz"; });
        addFloat (pid (z, "tremDepth"), "Tremolo Depth" + zn, { 0.0f, 1.0f }, s.tremDepth, pct);
        addFloat (pid (z, "chorus"), "Chorus" + zn, { 0.0f, 1.0f }, s.chorus, pct);
        addFloat (pid (z, "wobble"), "Tape Wobble" + zn, { 0.0f, 1.0f }, s.wobble, pct);
        addFloat (pid (z, "age"), "Tape Age" + zn, { 0.0f, 1.0f }, s.age, pct);
        addFloat (pid (z, "hiss"), "Tape Hiss" + zn, { 0.0f, 1.0f }, s.hiss, pct);
        addFloat (pid (z, "reverb"), "Reverb" + zn, { 0.0f, 1.0f }, s.reverb, pct);
        addFloat (pid (z, "volume"), "Volume" + zn, { -24.0f, 6.0f }, 0.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    }

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "split", 1 }, "Split Mode", false));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "splitPoint", 1 }, "Split Point", 36, 84, 60,
                                                      AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 4); })));
    addFloat ("strum", "Strum", { 0.0f, 150.0f }, 18.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "strumDir", 1 }, "Strum Direction", StringArray { "Up", "Down", "Alternate" }, 0));
    addFloat ("humanize", "Humanize", { 0.0f, 1.0f }, 0.25f, pct);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bass", 1 }, "Bass Note", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "voiceLeading", 1 }, "Voice Leading", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "chordMode", 1 }, "Chord Mode", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voicing", 1 }, "Voicing Style", StringArray { "Close", "Open", "Drop 2", "Rootless" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "topNote", 1 }, "Top Note", StringArray { "Auto", "Root", "3rd", "5th", "7th", "9th", "11th", "13th" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "suggest", 1 }, "Next-Chord Suggestions", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "midiOut", 1 }, "MIDI Output", StringArray { "Off", "Sound + MIDI", "MIDI only" }, 0));
    return { p.begin(), p.end() };
}

SoulKeysProcessor::SoulKeysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", createLayout())
{
    presetSets(); // build the 50 chord sets now, not on the audio thread
    reloadLibrary();
    for (int z = 0; z < numZones; ++z)
    {
        auto& zone = zones[(size_t) z];
        for (int i = 0; i < 40; ++i)
            zone.synth.addVoice (new KeysVoice (zone.voice));
        zone.synth.addSound (new KeysSound());
        zone.synth.setNoteStealingEnabled (true);
        auto get = [&] (const char* base) { return apvts.getRawParameterValue (pid (z, base)); };
        zone.p = { get ("chordSet"), get ("extensions"), get ("octave"), get ("key"), get ("scale"), get ("sound"),
                   get ("bell"), get ("bark"), get ("decay"), get ("tone"), get ("tremRate"), get ("tremDepth"),
                   get ("chorus"), get ("wobble"), get ("age"), get ("hiss"), get ("reverb"), get ("volume") };
    }
    splitParam = apvts.getRawParameterValue ("split");
    splitPointParam = apvts.getRawParameterValue ("splitPoint");
    pending.reserve (512);
    lastUpper.reserve (16);
    for (auto& v : notesFromKey) v.reserve (12);
}

bool SoulKeysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SoulKeysProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (auto& z : zones)
    {
        z.synth.setCurrentPlaybackSampleRate (sampleRate);
        z.fx.prepare (sampleRate, samplesPerBlock);
        z.buffer.setSize (2, samplesPerBlock);
        z.refs.fill (0);
        z.scaleKey = z.scaleIndex = -1;
    }
    keyboardState.reset();
    midiOut.ensureSize (8192);
    pending.clear();
    for (auto& v : notesFromKey) v.clear();
    blockStart = 0.0;
}

//==============================================================================
// Chord sets

int SoulKeysProcessor::numChordSets() const
{
    const juce::ScopedLock sl (chordLock);
    return libraryStart + (int) library.size();
}

void SoulKeysProcessor::reloadLibrary()
{
    std::vector<ChordSet> sets;
    auto files = PresetFile::folder().findChildFiles (juce::File::findFiles, false, "*" + PresetFile::extension());
    files.sort();
    for (const auto& f : files)
    {
        ChordSet set;
        juce::String error;
        if (PresetFile::load (f, set, error))
            sets.push_back (set);
        if ((int) sets.size() >= maxLibrary) break;
    }
    const juce::ScopedLock sl (chordLock);
    library = std::move (sets);
}

juce::String SoulKeysProcessor::importPreset (const juce::File& file, int zone)
{
    ChordSet set;
    juce::String error;
    if (! PresetFile::load (file, set, error))
        return error;
    // Keep a copy in the presets folder so it's there next time
    const auto dest = PresetFile::folder().getChildFile (file.getFileName());
    if (dest != file)
        file.copyFileTo (dest);
    reloadLibrary();
    const juce::ScopedLock sl (chordLock);
    for (size_t i = 0; i < library.size(); ++i)
        if (library[i].name == set.name)
        {
            auto* p = apvts.getParameter (pid (zone, "chordSet"));
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (libraryStart + (int) i)));
            p->endChangeGesture();
            break;
        }
    return {};
}

ChordSet SoulKeysProcessor::zoneChordsAsSet (int zone) const
{
    ChordSet set;
    for (int pc = 0; pc < 12; ++pc)
    {
        auto c = pickChord (zone, pc, true);
        if (c.root >= 0 && c.quality >= 0)
        {
            c.notes = Voicing::make (c.root, c.quality, true, 62, nullptr);
            c.name = Voicing::name (c.root, c.quality);
        }
        set.chords[(size_t) pc] = c;
    }
    set.tonic = set.chords[0].root;
    return set;
}

juce::String SoulKeysProcessor::saveZoneAsPreset (int zone, const juce::String& name)
{
    auto set = zoneChordsAsSet (zone);
    set.name = name;
    juce::String error;
    const auto file = PresetFile::save (set, error);
    if (error.isNotEmpty())
        return error;
    return importPreset (file, zone);
}

juce::String SoulKeysProcessor::chordSetName (int zone, int index) const
{
    if (index == scaleSlot) return "Key & Scale chords";
    if (index >= libraryStart)
    {
        const juce::ScopedLock sl (chordLock);
        const auto i = (size_t) (index - libraryStart);
        return i < library.size() ? "My: " + library[i].name : "(empty)";
    }
    if (index >= fileSlot)
    {
        const juce::ScopedLock sl (chordLock);
        const auto& u = zones[(size_t) zone].user;
        return u.chords[0].isEmpty() ? "From file (load a MIDI or audio file)" : u.name;
    }
    return presetSets()[(size_t) juce::jlimit (0, ChordPresets::count() - 1, index)].name;
}

ChordSet SoulKeysProcessor::getChordSet (int zone, int index) const
{
    if (index == scaleSlot)
    {
        const int key = juce::roundToInt (param (zones[(size_t) zone].p.key));
        return ScaleChords::make (key == 0 ? 0 : key - 1, juce::roundToInt (param (zones[(size_t) zone].p.scale)));
    }
    if (index >= libraryStart)
    {
        const juce::ScopedLock sl (chordLock);
        const auto i = (size_t) (index - libraryStart);
        if (i < library.size())
            return library[i];
    }
    else if (index >= fileSlot)
    {
        const juce::ScopedLock sl (chordLock);
        if (! zones[(size_t) zone].user.chords[0].isEmpty())
            return zones[(size_t) zone].user;
    }
    return presetSets()[(size_t) juce::jlimit (0, ChordPresets::count() - 1, index)];
}

void SoulKeysProcessor::setUserChords (int zone, const ChordSet& set)
{
    {
        const juce::ScopedLock sl (chordLock);
        zones[(size_t) zone].user = set;
    }
    auto* p = apvts.getParameter (pid (zone, "chordSet"));
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) fileSlot));
    p->endChangeGesture();
}

bool SoulKeysProcessor::hasUserChords (int zone) const
{
    const juce::ScopedLock sl (chordLock);
    return ! zones[(size_t) zone].user.chords[0].isEmpty();
}

void SoulKeysProcessor::applySoundPreset (int zone, int index)
{
    const auto& s = soundPresets()[(size_t) juce::jlimit (0, (int) soundPresets().size() - 1, index)];
    auto set = [this, zone] (const char* id, float v)
    {
        auto* p = apvts.getParameter (pid (zone, id));
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    };
    set ("sound", (float) index);
    set ("bell", s.bell); set ("bark", s.bark); set ("decay", s.decay); set ("tone", s.tone);
    set ("tremRate", s.tremRate); set ("tremDepth", s.tremDepth); set ("chorus", s.chorus);
    set ("wobble", s.wobble); set ("age", s.age); set ("hiss", s.hiss); set ("reverb", s.reverb);
}

void SoulKeysProcessor::swapZones()
{
    for (auto* base : { "chordSet", "extensions", "octave", "key", "scale" })
    {
        auto* a = apvts.getParameter (pid (0, base));
        auto* b = apvts.getParameter (pid (1, base));
        const float va = a->getValue(), vb = b->getValue();
        a->beginChangeGesture(); a->setValueNotifyingHost (vb); a->endChangeGesture();
        b->beginChangeGesture(); b->setValueNotifyingHost (va); b->endChangeGesture();
    }
    const juce::ScopedLock sl (chordLock);
    std::swap (zones[0].user, zones[1].user);
}

//==============================================================================
// Turning one key into a chord

Chord SoulKeysProcessor::pickChord (int zoneIndex, int pc, bool forDisplay) const
{
    const auto& zone = zones[(size_t) zoneIndex];
    const int index = juce::roundToInt (param (zone.p.chordSet));
    const int keyChoice = juce::roundToInt (param (zone.p.key));
    Chord c;
    int tonic = -1;

    if (index == scaleSlot)
    {
        if (forDisplay)
            return getChordSet (zoneIndex, index).chords[(size_t) pc]; // built in the key already
        return zone.scaleSet.chords[(size_t) pc];
    }
    if (index >= libraryStart)
    {
        const juce::ScopedTryLock sl (chordLock);
        const auto i = (size_t) (index - libraryStart);
        if (sl.isLocked() && i < library.size())
        {
            c = library[i].chords[(size_t) pc];
            tonic = library[i].tonic;
        }
    }
    else if (index >= fileSlot)
    {
        const juce::ScopedTryLock sl (chordLock);
        if (sl.isLocked() && ! zone.user.chords[0].isEmpty())
        {
            c = zone.user.chords[(size_t) pc];
            tonic = zone.user.tonic;
        }
    }
    if (c.isEmpty())
    {
        const auto& set = presetSets()[(size_t) juce::jlimit (0, ChordPresets::count() - 1, index >= fileSlot ? 0 : index)];
        c = set.chords[(size_t) pc];
        tonic = set.tonic;
    }

    // Move to the chosen key
    if (keyChoice > 0 && tonic >= 0)
    {
        int shift = ((keyChoice - 1 - tonic) % 12 + 12) % 12;
        if (shift > 6) shift -= 12;
        if (c.root >= 0) c.root = (c.root + shift + 12) % 12;
        else for (auto& n : c.notes) n += shift;
    }
    return c;
}

Chord SoulKeysProcessor::resolveChord (int key, int zoneIndex, std::vector<int>* previousUpper, bool forDisplay) const
{
    const auto& zp = zones[(size_t) zoneIndex].p;
    const int pc = key % 12;
    const bool split = splitOn();
    const int octave = juce::roundToInt (param (zp.octave));
    // In split mode both zones play in the same comfortable register; otherwise the key's octave sets it
    const int shift = (split ? 0 : (key / 12 - 5) * 12) + octave * 12;
    const int ext = juce::roundToInt (param (zp.extensions));
    const bool bass = param ("bass") > 0.5f;
    const bool leading = param ("voiceLeading") > 0.5f;

    Chord c = pickChord (zoneIndex, pc, forDisplay);
    Chord out;
    if (c.root >= 0 && c.quality >= 0)
    {
        const int q = ext == 0 ? c.quality : ChordQ::atLevel (c.quality, ext == 1 ? 7 : ext == 2 ? 9 : 11);
        const int centre = 62 + shift;
        const std::vector<int>* prev = nullptr;
        if (leading && previousUpper != nullptr && ! previousUpper->empty())
        {
            float mean = 0.0f;
            for (int n : *previousUpper) mean += (float) n;
            mean /= (float) previousUpper->size();
            if (std::abs (mean - (float) centre) < 9.0f)
                prev = previousUpper;
        }
        out.notes = Voicing::make (c.root, q, bass, centre, prev, 36 + juce::jlimit (-24, 24, shift),
                                   juce::roundToInt (param ("voicing")), juce::roundToInt (param ("topNote")));
        out.root = c.root;
        out.quality = q;
        out.name = Voicing::name (c.root, q);
        if (previousUpper != nullptr)
        {
            previousUpper->clear();
            for (int n : out.notes)
                if (n >= centre - 14) previousUpper->push_back (n);
        }
    }
    else
    {
        out = c;
        for (auto& n : out.notes) n += shift;
        if (! bass && out.notes.size() > 3)
            out.notes.erase (out.notes.begin());
    }
    for (auto& n : out.notes) n = juce::jlimit (21, 108, n);
    std::sort (out.notes.begin(), out.notes.end());
    out.notes.erase (std::unique (out.notes.begin(), out.notes.end()), out.notes.end());
    return out;
}

Chord SoulKeysProcessor::chordForKey (int pitchClass, int zone) const
{
    return resolveChord (60 + pitchClass, zone, nullptr, true);
}

void SoulKeysProcessor::handleKeyOn (int key, float velocity, int samplePos)
{
    handleKeyOff (key, samplePos); // a key pressed again restarts its chord
    const int zone = zoneOf (key);
    keyZone[(size_t) key] = zone;
    auto notes = resolveChord (key, zone, &lastUpper, false).notes;
    const int dir = juce::roundToInt (param ("strumDir"));
    const bool up = dir == 0 || (dir == 2 && strumUp);
    strumUp = ! strumUp;
    if (! up)
        std::reverse (notes.begin(), notes.end());

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const double strum = param ("strum") * 0.001 * sr;
    const float human = param ("humanize");
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const double jitter = human * rng.nextFloat() * 0.012 * sr;
        const float v = juce::jlimit (0.05f, 1.0f, velocity * (1.0f + human * 0.3f * (rng.nextFloat() - 0.5f)));
        pending.push_back ({ blockStart + samplePos + (double) i * strum + jitter, notes[i], v, key, zone });
    }
    lastKeyPc[zone] = key % 12;
    keyHeld[zone][key % 12] = true;
}

void SoulKeysProcessor::handleKeyOff (int key, int samplePos)
{
    pending.erase (std::remove_if (pending.begin(), pending.end(), [key] (const Pending& p) { return p.sourceKey == key; }), pending.end());
    const int zone = keyZone[(size_t) key];
    auto& z = zones[(size_t) zone];
    for (int n : notesFromKey[(size_t) key])
        if (z.refs[(size_t) n] > 0 && --z.refs[(size_t) n] == 0)
            z.midi.addEvent (juce::MidiMessage::noteOff (1, n), samplePos);
    notesFromKey[(size_t) key].clear();
    keyHeld[zone][key % 12] = false;
}

void SoulKeysProcessor::emitDue (int numSamples)
{
    const double end = blockStart + numSamples;
    for (auto it = pending.begin(); it != pending.end();)
    {
        if (it->time < end)
        {
            auto& z = zones[(size_t) it->zone];
            const int pos = juce::jlimit (0, numSamples - 1, (int) (it->time - blockStart));
            z.midi.addEvent (juce::MidiMessage::noteOn (1, it->note, it->velocity), pos);
            ++z.refs[(size_t) it->note];
            notesFromKey[(size_t) it->sourceKey].push_back (it->note);
            it = pending.erase (it);
        }
        else
            ++it;
    }
}

void SoulKeysProcessor::startRecording()
{
    recFifo.reset();
    recording = true;
}

void SoulKeysProcessor::stopRecording()
{
    recording = false;
}

void SoulKeysProcessor::drainRecording (std::vector<RecEvent>& into)
{
    const auto scope = recFifo.read (recFifo.getNumReady());
    for (int i = 0; i < scope.blockSize1; ++i) into.push_back (recBuffer[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) into.push_back (recBuffer[(size_t) (scope.startIndex2 + i)]);
}

void SoulKeysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    for (auto& z : zones)
    {
        const int sound = juce::jlimit (0, (int) soundPresets().size() - 1, juce::roundToInt (param (z.p.sound)));
        z.voice.engine = soundPresets()[(size_t) sound].engine;
        z.voice.bell = param (z.p.bell);
        z.voice.bark = param (z.p.bark);
        z.voice.decay = param (z.p.decay);
        z.midi.clear();
        // Rebuild the "Key & Scale" chords only when the key or scale changes
        const int key = juce::roundToInt (param (z.p.key)), scale = juce::roundToInt (param (z.p.scale));
        if (key != z.scaleKey || scale != z.scaleIndex)
        {
            z.scaleSet = ScaleChords::make (key == 0 ? 0 : key - 1, scale);
            z.scaleKey = key;
            z.scaleIndex = scale;
        }
        if (z.buffer.getNumSamples() < n)
            z.buffer.setSize (2, n, false, false, true);
    }

    const bool chordMode = param ("chordMode") > 0.5f;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (chordMode && m.isNoteOn())
            handleKeyOn (m.getNoteNumber(), m.getFloatVelocity(), meta.samplePosition);
        else if (chordMode && m.isNoteOff())
            handleKeyOff (m.getNoteNumber(), meta.samplePosition);
        else if (m.isNoteOnOrOff())
            zones[(size_t) zoneOf (m.getNoteNumber())].midi.addEvent (m, meta.samplePosition);
        else
            for (auto& z : zones) z.midi.addEvent (m, meta.samplePosition); // pedal etc. goes to both zones
    }
    emitDue (n);

    // Capture the notes that actually play (whole chords) for the recorder
    if (recording.load())
    {
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        for (auto& z : zones)
            for (const auto meta : z.midi)
            {
                const auto m = meta.getMessage();
                if (! m.isNoteOnOrOff()) continue;
                const auto scope = recFifo.write (1);
                if (scope.blockSize1 > 0)
                    recBuffer[(size_t) scope.startIndex1] = { (blockStart + meta.samplePosition) / sr, m.getNoteNumber(), m.getFloatVelocity(), m.isNoteOn() };
            }
    }

    // MIDI output: the chords go out as MIDI (zone A on channel 1, zone B on channel 2)
    const int midiMode = juce::roundToInt (param ("midiOut"));
    midiOut.clear();
    if (midiMode > 0)
        for (int zi = 0; zi < numZones; ++zi)
            for (const auto meta : zones[(size_t) zi].midi)
            {
                auto m = meta.getMessage();
                if (m.getChannel() > 0) m.setChannel (zi + 1);
                midiOut.addEvent (m, meta.samplePosition);
            }

    for (int zi = 0; zi < numZones; ++zi)
    {
        auto& z = zones[(size_t) zi];
        if (midiMode == 2) // MIDI only: the internal sound stays silent
        {
            z.midi.clear();
            continue;
        }
        bool active = false;
        for (int v = 0; v < z.synth.getNumVoices(); ++v)
            active = active || z.synth.getVoice (v)->isVoiceActive();
        // Zone B is skipped completely while split mode is off and it has nothing ringing
        if (zi == 1 && ! splitOn() && ! active && z.midi.isEmpty())
            continue;
        z.buffer.clear (0, n);
        juce::AudioBuffer<float> view (z.buffer.getArrayOfWritePointers(), 2, n);
        z.synth.renderNextBlock (view, z.midi, 0, n);
        z.fxSettings = { param (z.p.tone), param (z.p.tremRate), param (z.p.tremDepth), param (z.p.chorus),
                         param (z.p.wobble), param (z.p.age), param (z.p.hiss), param (z.p.reverb), param (z.p.volume),
                         active || ! z.midi.isEmpty() };
        z.fx.process (view, z.fxSettings);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom (ch, 0, view, ch, 0, n);
    }
    midi.swapWith (midiOut);
    blockStart += n;
}

void SoulKeysProcessor::switchCompare (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot) return;
    compareSlots[(size_t) activeSlot] = apvts.copyState();
    if (! compareSlots[(size_t) slot].isValid())
        compareSlots[(size_t) slot] = compareSlots[(size_t) activeSlot].createCopy();
    apvts.replaceState (compareSlots[(size_t) slot].createCopy());
    activeSlot = slot;
}

void SoulKeysProcessor::copyCompareSlot()
{
    compareSlots[(size_t) (1 - activeSlot)] = apvts.copyState();
}

//==============================================================================
// Saving: parameters, skin, plus the chords pulled from files

static void writeSet (juce::XmlElement& parent, const juce::String& tag, const ChordSet& set)
{
    if (set.chords[0].isEmpty()) return;
    auto* user = parent.createNewChildElement (tag);
    user->setAttribute ("name", set.name);
    user->setAttribute ("tonic", set.tonic);
    for (const auto& c : set.chords)
    {
        auto* e = user->createNewChildElement ("Chord");
        e->setAttribute ("name", c.name);
        e->setAttribute ("root", c.root);
        e->setAttribute ("quality", c.quality);
        juce::StringArray notes;
        for (int nn : c.notes) notes.add (juce::String (nn));
        e->setAttribute ("notes", notes.joinIntoString (" "));
    }
}

static ChordSet readSet (const juce::XmlElement& user)
{
    ChordSet set;
    set.name = user.getStringAttribute ("name");
    set.tonic = user.getIntAttribute ("tonic", -1);
    int i = 0;
    for (auto* e : user.getChildIterator())
    {
        if (i >= 12) break;
        auto& c = set.chords[(size_t) i++];
        c.name = e->getStringAttribute ("name");
        c.root = e->getIntAttribute ("root", -1);
        c.quality = e->getIntAttribute ("quality", -1);
        for (auto& t : juce::StringArray::fromTokens (e->getStringAttribute ("notes"), " ", ""))
            if (t.isNotEmpty()) c.notes.push_back (t.getIntValue());
    }
    return set;
}

void SoulKeysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr) return;
    {
        const juce::ScopedLock sl (chordLock);
        writeSet (*xml, "UserChords", zones[0].user);
        writeSet (*xml, "UserChordsB", zones[1].user);
        for (int z = 0; z < numZones; ++z)
        {
            const int index = juce::roundToInt (param (zones[(size_t) z].p.chordSet));
            if (index >= libraryStart && (size_t) (index - libraryStart) < library.size())
                xml->setAttribute (z == 0 ? "libraryPresetA" : "libraryPresetB", library[(size_t) (index - libraryStart)].name);
        }
    }
    copyXmlToBinary (*xml, destData);
}

void SoulKeysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    for (auto [tag, zone] : { std::pair { "UserChords", 0 }, { "UserChordsB", 1 } })
        if (auto* user = xml->getChildByName (tag))
        {
            auto set = readSet (*user);
            {
                const juce::ScopedLock sl (chordLock);
                zones[(size_t) zone].user = set;
            }
            xml->removeChildElement (user, true);
        }
    const juce::String libNames[] { xml->getStringAttribute ("libraryPresetA"), xml->getStringAttribute ("libraryPresetB") };
    xml->removeAttribute ("libraryPresetA");
    xml->removeAttribute ("libraryPresetB");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // "My Presets" can be in a different order on another computer: find them by name
    reloadLibrary();
    for (int z = 0; z < numZones; ++z)
    {
        if (libNames[z].isEmpty()) continue;
        int found = -1;
        {
            const juce::ScopedLock sl (chordLock);
            for (size_t i = 0; i < library.size(); ++i)
                if (library[i].name == libNames[z]) found = libraryStart + (int) i;
        }
        auto* p = apvts.getParameter (pid (z, "chordSet"));
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (found >= 0 ? found : 0)));
    }
}

juce::AudioProcessorEditor* SoulKeysProcessor::createEditor()
{
    return new SoulKeysEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoulKeysProcessor();
}
