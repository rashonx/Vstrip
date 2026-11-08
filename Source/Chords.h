#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// Chord qualities: the upper-structure intervals (above the root) that make each chord lush.
namespace ChordQ
{
    enum Family { major, minor, dominant, sus, halfDim, dim };

    enum Quality
    {
        M7, M9, M9s11, M69, m7, m9, m11, m69, mM9,
        d7, d9, d13, d7s9, d13b9, d7alt, d9s11, d11,
        sus7, sus9, sus13, m7b5, h9, m11b5, dim7, add9, M7s5,
        numQualities
    };

    struct Info
    {
        const char* suffix;
        std::vector<int> upper;  // intervals above the root, played by the right hand
        Family family;
        bool naturalFifth;
    };

    const Info& info (int quality);

    // The chord of the same family at a 7th, 9th or 11th (level 7, 9 or 11)
    int atLevel (int quality, int level);

    juce::String noteName (int pitchClass);
}

struct Chord
{
    juce::String name;
    int root = -1;        // pitch class, or -1 when unknown
    int quality = -1;     // ChordQ::Quality, or -1 when unknown (then only `notes` is used)
    std::vector<int> notes; // full voicing for the key at middle C's octave

    bool isEmpty() const { return notes.empty(); }
};

namespace Voicing
{
    // Root in the bass (C2-B2) plus the upper structure arranged around `centre`.
    // With `previousUpper`, picks the inversion that moves the least (voice leading).
    enum Style { close = 0, open, drop2, rootless };
    // topDegree: 0 auto, 1 root, 2 3rd, 3 5th, 4 7th, 5 9th, 6 11th, 7 13th (that chord tone goes on top)
    std::vector<int> make (int root, int quality, bool includeBass, int centre, const std::vector<int>* previousUpper,
                           int bassLow = 36, int style = close, int topDegree = 0);

    juce::String name (int root, int quality, int bassPc = -1);

    // Best guess at the root and quality of a set of notes (lowest note is the bass)
    Chord identify (const std::vector<int>& notes);

    // Reads a chord name like "Ebmaj9", "F#m11", "C13(b9)", "Bbø", "G7sus" (empty Chord if unreadable)
    Chord parseName (const juce::String& name);
}

// 12 chords: one per key of the octave (index = pitch class: 0 = C, 1 = C#, ...)
struct ChordSet
{
    juce::String name;
    int tonic = -1;   // key the set is written in (pitch class), -1 if unknown
    std::array<Chord, 12> chords;
};

// Lush diatonic chords built from a key and scale (white keys = degrees I-VII)
namespace ScaleChords
{
    juce::StringArray scaleNames();
    ChordSet make (int keyPc, int scaleIndex);
}

// Which keys would sound good after the last chord (higher score = better)
namespace Suggest
{
    std::array<float, 12> score (const Chord& last, const std::array<Chord, 12>& options);
}

// Moves a whole chord set by some semitones (roots, names and voicings)
ChordSet transposeSet (const ChordSet& set, int semitones);

namespace ChordPresets
{
    int count();
    ChordSet get (int index);

    static constexpr std::array<int, 7> whitePcs { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr std::array<int, 5> blackPcs { 1, 3, 6, 8, 10 };

    // Fills the black keys with chords leading into the next white key.
    // flavours: one letter per black key - S secondary dominant, T tritone sub,
    // C chromatic slide, D passing diminished, U sus dominant.
    void fillBlackKeys (ChordSet& set, const char* flavours);
}
