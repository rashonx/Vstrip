#include "ChordExtractor.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <set>

namespace ChordExtractor
{
    bool isMidiFile (const juce::File& f)
    {
        return f.hasFileExtension ("mid;midi;smf");
    }

    juce::String supportedPatterns()
    {
        return "*.mid;*.midi;*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg";
    }

    // Moves a voicing up or down by octaves so it sits around middle C
    static std::vector<int> centre (std::vector<int> notes)
    {
        if (notes.empty()) return notes;
        std::sort (notes.begin(), notes.end());
        float mean = 0.0f;
        for (int n : notes) mean += (float) n;
        mean /= (float) notes.size();
        const int shift = 12 * juce::roundToInt ((58.0f - mean) / 12.0f);
        for (auto& n : notes) n = juce::jlimit (24, 100, n + shift);
        return notes;
    }

    // Puts up to 12 chords on the keys; repeats them if there are fewer than 7
    static ChordSet layOut (const std::vector<Chord>& found, const juce::String& name)
    {
        ChordSet set;
        set.name = "From file: " + name;
        for (size_t i = 0; i < 7; ++i)
            set.chords[(size_t) ChordPresets::whitePcs[i]] = found[i % found.size()];
        if (found.size() > 7)
        {
            for (size_t i = 7; i < std::min<size_t> (12, found.size()); ++i)
                set.chords[(size_t) ChordPresets::blackPcs[i - 7]] = found[i];
            // Any black keys still empty get related chords
            ChordSet related = set;
            ChordPresets::fillBlackKeys (related, "SSSSS");
            for (size_t i = found.size() - 7; i < 5; ++i)
                set.chords[(size_t) ChordPresets::blackPcs[i]] = related.chords[(size_t) ChordPresets::blackPcs[i]];
        }
        else
            ChordPresets::fillBlackKeys (set, "STCSD");
        return set;
    }

    Result fromMidi (const juce::MidiFile& midiIn, const juce::String& name)
    {
        Result result;
        juce::MidiFile midi (midiIn);
        midi.convertTimestampTicksToSeconds();

        struct Note { double start, end; int pitch; };
        std::vector<Note> notes;
        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*midi.getTrack (t));
            seq.updateMatchedPairs();
            for (auto* e : seq)
            {
                const auto& m = e->message;
                if (! m.isNoteOn() || m.getChannel() == 10) // skip drums
                    continue;
                const double end = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : m.getTimeStamp() + 0.5;
                notes.push_back ({ m.getTimeStamp(), end, m.getNoteNumber() });
            }
        }
        if (notes.empty())
        {
            result.error = "No notes found in this MIDI file";
            return result;
        }
        std::sort (notes.begin(), notes.end(), [] (auto& a, auto& b) { return a.start < b.start; });

        // Look at what's sounding just after each group of notes starts
        std::vector<Chord> found;
        std::set<int> previous;
        double lastGroup = -1.0;
        for (const auto& n : notes)
        {
            if (n.start - lastGroup < 0.06)
                continue;
            lastGroup = n.start;
            const double t = n.start + 0.05;
            std::vector<int> sounding;
            std::set<int> pcs;
            for (const auto& o : notes)
                if (o.start <= t && o.end > t + 0.03)
                {
                    sounding.push_back (o.pitch);
                    pcs.insert (o.pitch % 12);
                }
            if (pcs.size() < 3 || pcs == previous)
                continue;
            previous = pcs;

            std::sort (sounding.begin(), sounding.end());
            sounding.erase (std::unique (sounding.begin(), sounding.end()), sounding.end());
            auto c = Voicing::identify (sounding);
            c.notes = centre (sounding);
            // Skip chords we already have
            const bool duplicate = std::any_of (found.begin(), found.end(), [&] (const Chord& f) { return f.name == c.name; });
            if (! duplicate)
                found.push_back (c);
            if (found.size() >= 12)
                break;
        }

        if (found.empty())
        {
            result.error = "No chords found (the file needs at least three notes playing together)";
            return result;
        }
        result.chordsFound = (int) found.size();
        result.set = layOut (found, name);
        return result;
    }

    Result fromAudio (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name)
    {
        Result result;
        const int fftOrder = 13, fftSize = 1 << fftOrder;
        const int hop = fftSize / 2;
        const int total = audio.getNumSamples();
        if (total < fftSize * 2)
        {
            result.error = "The audio file is too short";
            return result;
        }

        juce::dsp::FFT fft (fftOrder);
        std::vector<float> window ((size_t) fftSize), frame ((size_t) fftSize * 2);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

        // 1. Chroma (energy per pitch class) and bass chroma for every frame
        struct Frame { std::array<float, 12> chroma {}, bass {}; float energy = 0.0f; };
        std::vector<Frame> frames;
        for (int start = 0; start + fftSize <= total; start += hop)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            for (int i = 0; i < fftSize; ++i)
            {
                float s = 0.0f;
                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                    s += audio.getSample (ch, start + i);
                frame[(size_t) i] = s * window[(size_t) i];
            }
            fft.performFrequencyOnlyForwardTransform (frame.data(), true);
            Frame f;
            for (int k = 1; k < fftSize / 2; ++k)
            {
                const double hz = (double) k * sampleRate / fftSize;
                if (hz < 40.0 || hz > 2500.0) continue;
                const float e = frame[(size_t) k] * frame[(size_t) k];
                const int pc = ((int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0)) % 12 + 12) % 12;
                if (hz >= 65.0) f.chroma[(size_t) pc] += e;
                if (hz < 260.0) f.bass[(size_t) pc] += e;
                f.energy += e;
            }
            frames.push_back (f);
        }

        // 2. Match each (smoothed) frame against chord templates
        struct Template { int root, quality; std::array<float, 12> shape {}; };
        std::vector<Template> templates;
        const std::array<std::pair<int, std::vector<int>>, 5> kinds {{
            { ChordQ::M7,  { 0, 4, 7 } },       // plain major
            { ChordQ::m7,  { 0, 3, 7 } },       // plain minor
            { ChordQ::d7,  { 0, 4, 7, 10 } },
            { ChordQ::M7,  { 0, 4, 7, 11 } },
            { ChordQ::m7,  { 0, 3, 7, 10 } },
        }};
        for (int r = 0; r < 12; ++r)
            for (const auto& [q, ivs] : kinds)
            {
                Template t { r, q, {} };
                for (int iv : ivs) t.shape[(size_t) ((r + iv) % 12)] = 1.0f;
                templates.push_back (t);
            }

        float maxEnergy = 0.0f;
        for (auto& f : frames) maxEnergy = std::max (maxEnergy, f.energy);
        const int smooth = std::max (1, (int) std::lround (0.4 * sampleRate / hop));
        std::vector<int> labels (frames.size(), -1);
        for (size_t i = 0; i < frames.size(); ++i)
        {
            std::array<float, 12> c {}, b {};
            float energy = 0.0f;
            for (size_t j = (i >= (size_t) smooth ? i - (size_t) smooth : 0); j <= std::min (frames.size() - 1, i + (size_t) smooth); ++j)
            {
                for (size_t p = 0; p < 12; ++p) { c[p] += std::sqrt (frames[j].chroma[p]); b[p] += frames[j].bass[p]; }
                energy += frames[j].energy;
            }
            if (frames[i].energy < maxEnergy * 0.003f)
                continue; // silence
            float norm = 0.0f;
            for (float v : c) norm += v * v;
            norm = std::sqrt (std::max (norm, 1.0e-12f));
            const int bassPc = (int) (std::max_element (b.begin(), b.end()) - b.begin());
            float best = -1.0f;
            for (size_t t = 0; t < templates.size(); ++t)
            {
                float dot = 0.0f, tn = 0.0f;
                for (size_t p = 0; p < 12; ++p) { dot += c[p] * templates[t].shape[p]; tn += templates[t].shape[p]; }
                float score = dot / (norm * std::sqrt (tn));
                if (templates[t].root == bassPc) score += 0.08f;
                if (score > best) { best = score; labels[i] = (int) t; }
            }
        }

        // 3. Keep chords that last at least ~0.7 s, in order of appearance, made lush
        std::vector<Chord> found;
        const int minFrames = std::max (2, (int) std::lround (0.7 * sampleRate / hop));
        size_t i = 0;
        while (i < labels.size())
        {
            size_t j = i;
            while (j < labels.size() && labels[j] == labels[i]) ++j;
            if (labels[i] >= 0 && (int) (j - i) >= minFrames)
            {
                const auto& t = templates[(size_t) labels[i]];
                const int lush = ChordQ::atLevel (t.quality, 9); // neo soul colour: major -> maj9, minor -> m9, dominant -> 9
                Chord c;
                c.root = t.root;
                c.quality = lush;
                c.name = Voicing::name (t.root, lush);
                c.notes = Voicing::make (t.root, lush, true, 62, nullptr);
                const bool duplicate = std::any_of (found.begin(), found.end(), [&] (const Chord& f) { return f.name == c.name; });
                if (! duplicate)
                    found.push_back (c);
                if (found.size() >= 12)
                    break;
            }
            i = j;
        }

        if (found.empty())
        {
            result.error = "Couldn't hear clear chords in this audio. Try a section with sustained piano or guitar chords.";
            return result;
        }
        result.chordsFound = (int) found.size();
        result.set = layOut (found, name);
        return result;
    }

    Result fromFile (const juce::File& file)
    {
        if (isMidiFile (file))
        {
            juce::FileInputStream in (file);
            juce::MidiFile midi;
            if (! in.openedOk() || ! midi.readFrom (in))
                return { {}, 0, "Couldn't read this MIDI file" };
            return fromMidi (midi, file.getFileNameWithoutExtension());
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            return { {}, 0, "Couldn't read this file. Use MIDI, WAV, AIFF, FLAC, MP3 or OGG." };
        // Analyse up to 6 minutes
        const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 360.0));
        juce::AudioBuffer<float> audio ((int) std::min (2u, reader->numChannels), length);
        reader->read (&audio, 0, length, 0, true, audio.getNumChannels() > 1);
        return fromAudio (audio, reader->sampleRate, file.getFileNameWithoutExtension());
    }
}
