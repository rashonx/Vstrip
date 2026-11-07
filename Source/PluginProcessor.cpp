#include "PluginProcessor.h"
#include "PluginEditor.h"

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
}

juce::AudioProcessorValueTreeState::ParameterLayout SoulKeysProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> fmt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "chordSet", 1 }, "Chord Set", 0, ChordPresets::count(), 0));
    StringArray sounds;
    for (auto& s : soundPresets()) sounds.add (s.name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "sound", 1 }, "Sound", sounds, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "extensions", 1 }, "Extensions", StringArray { "As preset", "7th", "9th", "11th" }, 0));

    const auto& s0 = soundPresets().front();
    addFloat ("bell", "Bell", { 0.0f, 1.0f }, s0.bell, pct);
    addFloat ("bark", "Bark", { 0.0f, 1.0f }, s0.bark, pct);
    addFloat ("decay", "Decay", { 0.5f, 2.0f }, s0.decay, [] (float v, int) { return String (v, 2) + "x"; });
    addFloat ("tone", "Tone", { 0.0f, 1.0f }, s0.tone, pct);
    addFloat ("tremRate", "Tremolo Rate", { 0.5f, 10.0f }, s0.tremRate, [] (float v, int) { return String (v, 1) + " Hz"; });
    addFloat ("tremDepth", "Tremolo Depth", { 0.0f, 1.0f }, s0.tremDepth, pct);
    addFloat ("chorus", "Chorus", { 0.0f, 1.0f }, s0.chorus, pct);
    addFloat ("wobble", "Tape Wobble", { 0.0f, 1.0f }, s0.wobble, pct);
    addFloat ("age", "Tape Age", { 0.0f, 1.0f }, s0.age, pct);
    addFloat ("hiss", "Tape Hiss", { 0.0f, 1.0f }, s0.hiss, pct);
    addFloat ("reverb", "Reverb", { 0.0f, 1.0f }, s0.reverb, pct);
    addFloat ("volume", "Volume", { -24.0f, 6.0f }, 0.0f, [] (float v, int) { return String (v, 1) + " dB"; });

    addFloat ("strum", "Strum", { 0.0f, 150.0f }, 18.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "strumDir", 1 }, "Strum Direction", StringArray { "Up", "Down", "Alternate" }, 0));
    addFloat ("humanize", "Humanize", { 0.0f, 1.0f }, 0.25f, pct);
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bass", 1 }, "Bass Note", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "voiceLeading", 1 }, "Voice Leading", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "chordMode", 1 }, "Chord Mode", true));
    return { p.begin(), p.end() };
}

SoulKeysProcessor::SoulKeysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    presetSets(); // build the 50 chord sets now, not on the audio thread
    for (int i = 0; i < 48; ++i)
        synth.addVoice (new KeysVoice (voiceSettings));
    synth.addSound (new KeysSound());
    synth.setNoteStealingEnabled (true);
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
    synth.setCurrentPlaybackSampleRate (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
    pending.clear();
    for (auto& v : notesFromKey) v.clear();
    noteRefs.fill (0);
    blockStart = 0.0;
}

//==============================================================================
// Chord sets

ChordSet SoulKeysProcessor::getChordSet (int index) const
{
    if (index >= ChordPresets::count())
    {
        const juce::ScopedLock sl (chordLock);
        if (! userChords.chords[0].isEmpty())
            return userChords;
        ChordSet empty = presetSets().front();
        empty.name = "From file (load a MIDI or audio file)";
        return empty;
    }
    return presetSets()[(size_t) juce::jlimit (0, ChordPresets::count() - 1, index)];
}

ChordSet SoulKeysProcessor::currentChordSet() const
{
    return getChordSet (juce::roundToInt (param ("chordSet")));
}

void SoulKeysProcessor::setUserChords (const ChordSet& set)
{
    {
        const juce::ScopedLock sl (chordLock);
        userChords = set;
    }
    auto* p = apvts.getParameter ("chordSet");
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) userSlot()));
    p->endChangeGesture();
}

bool SoulKeysProcessor::hasUserChords() const
{
    const juce::ScopedLock sl (chordLock);
    return ! userChords.chords[0].isEmpty();
}

void SoulKeysProcessor::applySoundPreset (int index)
{
    const auto& s = soundPresets()[(size_t) juce::jlimit (0, (int) soundPresets().size() - 1, index)];
    auto set = [this] (const char* id, float v)
    {
        auto* p = apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    };
    set ("sound", (float) index);
    set ("bell", s.bell); set ("bark", s.bark); set ("decay", s.decay); set ("tone", s.tone);
    set ("tremRate", s.tremRate); set ("tremDepth", s.tremDepth); set ("chorus", s.chorus);
    set ("wobble", s.wobble); set ("age", s.age); set ("hiss", s.hiss); set ("reverb", s.reverb);
}

//==============================================================================
// Turning one key into a chord

Chord SoulKeysProcessor::resolveChord (int key, std::vector<int>& previousUpper) const
{
    Chord out;
    const int pc = key % 12;
    const int shift = (key / 12 - 5) * 12 + juce::roundToInt (param ("octave")) * 12;
    const int ext = juce::roundToInt (param ("extensions"));
    const bool bass = param ("bass") > 0.5f;
    const bool leading = param ("voiceLeading") > 0.5f;
    const int setIndex = juce::roundToInt (param ("chordSet"));

    Chord c;
    {
        const juce::ScopedTryLock sl (chordLock);
        if (setIndex >= ChordPresets::count() && sl.isLocked() && ! userChords.chords[0].isEmpty())
            c = userChords.chords[(size_t) pc];
        else
            c = presetSets()[(size_t) juce::jlimit (0, ChordPresets::count() - 1, setIndex)].chords[(size_t) pc];
    }

    if (c.root >= 0 && c.quality >= 0)
    {
        const int q = ext == 0 ? c.quality : ChordQ::atLevel (c.quality, ext == 1 ? 7 : ext == 2 ? 9 : 11);
        const int centre = 62 + shift;
        const std::vector<int>* prev = nullptr;
        if (leading && ! previousUpper.empty())
        {
            float mean = 0.0f;
            for (int n : previousUpper) mean += (float) n;
            mean /= (float) previousUpper.size();
            if (std::abs (mean - (float) centre) < 9.0f)
                prev = &previousUpper;
        }
        out.notes = Voicing::make (c.root, q, bass, centre, prev, 36 + juce::jlimit (-24, 24, shift));
        out.root = c.root;
        out.quality = q;
        out.name = Voicing::name (c.root, q);
        // Remember the upper structure for voice leading
        previousUpper.clear();
        for (int n : out.notes)
            if (n >= centre - 14) previousUpper.push_back (n);
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

Chord SoulKeysProcessor::chordForKey (int pitchClass) const
{
    std::vector<int> none;
    return resolveChord (60 + pitchClass, none);
}

void SoulKeysProcessor::handleKeyOn (int key, float velocity, int samplePos)
{
    handleKeyOff (key, samplePos); // a key pressed again restarts its chord
    auto chord = resolveChord (key, lastUpper);
    auto notes = chord.notes;
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
        pending.push_back ({ blockStart + samplePos + (double) i * strum + jitter, notes[i], v, key });
    }
    lastKeyPc = key % 12;
    keyHeld[key % 12] = true;
}

void SoulKeysProcessor::handleKeyOff (int key, int samplePos)
{
    pending.erase (std::remove_if (pending.begin(), pending.end(), [key] (const Pending& p) { return p.sourceKey == key; }), pending.end());
    for (int n : notesFromKey[(size_t) key])
        if (noteRefs[(size_t) n] > 0 && --noteRefs[(size_t) n] == 0)
            expanded.addEvent (juce::MidiMessage::noteOff (1, n), samplePos);
    notesFromKey[(size_t) key].clear();
    keyHeld[key % 12] = false;
}

void SoulKeysProcessor::emitDue (juce::MidiBuffer& out, int numSamples)
{
    const double end = blockStart + numSamples;
    for (auto it = pending.begin(); it != pending.end();)
    {
        if (it->time < end)
        {
            const int pos = juce::jlimit (0, numSamples - 1, (int) (it->time - blockStart));
            out.addEvent (juce::MidiMessage::noteOn (1, it->note, it->velocity), pos);
            ++noteRefs[(size_t) it->note];
            notesFromKey[(size_t) it->sourceKey].push_back (it->note);
            it = pending.erase (it);
        }
        else
            ++it;
    }
}

void SoulKeysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    const int sound = juce::roundToInt (param ("sound"));
    voiceSettings.engine = soundPresets()[(size_t) juce::jlimit (0, (int) soundPresets().size() - 1, sound)].engine;
    voiceSettings.bell = param ("bell");
    voiceSettings.bark = param ("bark");
    voiceSettings.decay = param ("decay");

    const bool chordMode = param ("chordMode") > 0.5f;
    expanded.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (chordMode && m.isNoteOn())
            handleKeyOn (m.getNoteNumber(), m.getFloatVelocity(), meta.samplePosition);
        else if (chordMode && m.isNoteOff())
            handleKeyOff (m.getNoteNumber(), meta.samplePosition);
        else
            expanded.addEvent (m, meta.samplePosition);
    }
    emitDue (expanded, n);

    synth.renderNextBlock (buffer, expanded, 0, n);

    fxSettings.tone = param ("tone");
    fxSettings.tremRate = param ("tremRate");
    fxSettings.tremDepth = param ("tremDepth");
    fxSettings.chorus = param ("chorus");
    fxSettings.wobble = param ("wobble");
    fxSettings.age = param ("age");
    fxSettings.hiss = param ("hiss");
    fxSettings.reverb = param ("reverb");
    fxSettings.volume = param ("volume");
    fx.process (buffer, fxSettings);

    blockStart += n;
}

//==============================================================================
// Saving: parameters plus the chords pulled from a file

void SoulKeysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml == nullptr) return;
    {
        const juce::ScopedLock sl (chordLock);
        if (! userChords.chords[0].isEmpty())
        {
            auto* user = xml->createNewChildElement ("UserChords");
            user->setAttribute ("name", userChords.name);
            for (const auto& c : userChords.chords)
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
    }
    copyXmlToBinary (*xml, destData);
}

void SoulKeysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    if (auto* user = xml->getChildByName ("UserChords"))
    {
        ChordSet set;
        set.name = user->getStringAttribute ("name");
        int i = 0;
        for (auto* e : user->getChildIterator())
        {
            if (i >= 12) break;
            auto& c = set.chords[(size_t) i++];
            c.name = e->getStringAttribute ("name");
            c.root = e->getIntAttribute ("root", -1);
            c.quality = e->getIntAttribute ("quality", -1);
            for (auto& t : juce::StringArray::fromTokens (e->getStringAttribute ("notes"), " ", ""))
                if (t.isNotEmpty()) c.notes.push_back (t.getIntValue());
        }
        const juce::ScopedLock sl (chordLock);
        userChords = set;
        xml->removeChildElement (user, true);
    }
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SoulKeysProcessor::createEditor()
{
    return new SoulKeysEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoulKeysProcessor();
}
