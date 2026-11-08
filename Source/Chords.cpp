#include "Chords.h"
#include <algorithm>
#include <numeric>
#include <set>
#include <cstring>

namespace ChordQ
{
    const Info& info (int q)
    {
        static const std::vector<Info> table {
            { "maj7",      { 4, 7, 11 },          major,    true  },
            { "maj9",      { 4, 7, 11, 14 },      major,    true  },
            { "maj9(#11)", { 4, 11, 14, 18 },     major,    true  },
            { "6/9",       { 4, 7, 9, 14 },       major,    true  },
            { "m7",        { 3, 7, 10 },          minor,    true  },
            { "m9",        { 3, 7, 10, 14 },      minor,    true  },
            { "m11",       { 3, 10, 14, 17 },     minor,    true  },
            { "m6/9",      { 3, 7, 9, 14 },       minor,    true  },
            { "m(maj9)",   { 3, 7, 11, 14 },      minor,    true  },
            { "7",         { 4, 7, 10 },          dominant, true  },
            { "9",         { 4, 10, 14 },         dominant, true  },
            { "13",        { 4, 10, 14, 21 },     dominant, true  },
            { "7(#9)",     { 4, 10, 15 },         dominant, true  },
            { "13(b9)",    { 4, 10, 13, 21 },     dominant, true  },
            { "7alt",      { 4, 10, 15, 20 },     dominant, false },
            { "9(#11)",    { 4, 10, 14, 18 },     dominant, true  },
            { "11",        { 7, 10, 14, 17 },     dominant, true  },
            { "7sus4",     { 5, 7, 10 },          sus,      true  },
            { "9sus4",     { 5, 10, 14 },         sus,      true  },
            { "13sus4",    { 5, 10, 14, 21 },     sus,      true  },
            { "m7(b5)",    { 3, 6, 10 },          halfDim,  false },
            { "m9(b5)",    { 3, 6, 10, 14 },      halfDim,  false },
            { "m11(b5)",   { 3, 6, 10, 14, 17 },  halfDim,  false },
            { "dim7",      { 3, 6, 9 },           dim,      false },
            { "add9",      { 4, 7, 14 },          major,    true  },
            { "maj7(#5)",  { 4, 8, 11 },          major,    false },
        };
        return table[(size_t) juce::jlimit (0, (int) table.size() - 1, q)];
    }

    int atLevel (int q, int level)
    {
        const int l = level <= 7 ? 0 : level <= 9 ? 1 : 2;
        switch (info (q).family)
        {
            case major:    return std::array<int, 3> { M7, M9, M9s11 }[(size_t) l];
            case minor:    return std::array<int, 3> { m7, m9, m11 }[(size_t) l];
            case dominant: return std::array<int, 3> { d7, d9, d11 }[(size_t) l];
            case sus:      return std::array<int, 3> { sus7, sus9, d11 }[(size_t) l];
            case halfDim:  return std::array<int, 3> { m7b5, h9, m11b5 }[(size_t) l];
            case dim:
            default:       return dim7;
        }
    }

    juce::String noteName (int pc)
    {
        static const char* names[] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return names[((pc % 12) + 12) % 12];
    }
}

namespace Voicing
{
    // Which interval above the root plays the role of a chord degree (3rd, 5th, ...), or -1
    static int intervalForDegree (const ChordQ::Info& q, int degree)
    {
        auto has = [&] (std::initializer_list<int> options)
        {
            for (int o : options)
                for (int iv : q.upper)
                    if (iv % 12 == o % 12) return iv;
            return -1;
        };
        switch (degree)
        {
            case 1: return 12;
            case 2: { const int v = has ({ 4, 3, 5 }); return v >= 0 ? v : (q.family == ChordQ::minor || q.family == ChordQ::halfDim || q.family == ChordQ::dim ? 3 : 4); }
            case 3: { const int v = has ({ 7, 6, 8 }); return v >= 0 ? v : 7; }
            case 4: { const int v = has ({ 10, 11 }); return v >= 0 ? v : (q.family == ChordQ::major ? 11 : q.family == ChordQ::dim ? 9 : 10); }
            case 5: { const int v = has ({ 14, 13, 15 }); return v >= 0 ? v : 14; }
            case 6: { const int v = has ({ 17, 18 }); return v >= 0 ? v : (q.family == ChordQ::major || q.family == ChordQ::dominant ? 18 : 17); }
            case 7: { const int v = has ({ 21, 20, 9 }); return v >= 0 ? v : 21; }
            default: return -1;
        }
    }

    std::vector<int> make (int root, int quality, bool includeBass, int centre, const std::vector<int>* previousUpper, int bassLow, int style, int topDegree)
    {
        const auto& q = ChordQ::info (quality);
        std::vector<int> pcs;
        for (int iv : q.upper)
            pcs.push_back ((root + iv) % 12);

        // Top note: make sure that chord tone is in the voicing, then only accept voicings with it on top
        int topPc = -1;
        if (topDegree > 0)
        {
            const int iv = intervalForDegree (q, topDegree);
            if (iv >= 0)
            {
                topPc = (root + iv) % 12;
                if (std::find (pcs.begin(), pcs.end(), topPc) == pcs.end())
                    pcs.push_back (topPc);
            }
        }
        if (style == rootless)
            includeBass = false;

        // Try every rotation of the upper structure, stacked upwards; keep the best one
        std::vector<int> best;
        float bestScore = 1.0e9f;
        for (size_t r = 0; r < pcs.size(); ++r)
        {
            for (int startOct = 3; startOct <= 6; ++startOct)
            {
                std::vector<int> v;
                int note = startOct * 12 + pcs[r];
                v.push_back (note);
                for (size_t k = 1; k < pcs.size(); ++k)
                {
                    int next = (note / 12) * 12 + pcs[(r + k) % pcs.size()];
                    while (next <= note) next += 12;
                    v.push_back (next);
                    note = next;
                }
                if (topPc >= 0 && v.back() % 12 != topPc)
                    continue;
                const float mean = (float) std::accumulate (v.begin(), v.end(), 0) / (float) v.size();
                float score = std::abs (mean - (float) centre) + 0.15f * (float) (v.back() - v.front());
                if (previousUpper != nullptr && ! previousUpper->empty())
                {
                    // Smallest total movement from the previous chord
                    float move = 0.0f;
                    for (int n : v)
                    {
                        int nearest = 99;
                        for (int p : *previousUpper)
                            nearest = std::min (nearest, std::abs (n - p));
                        move += (float) nearest;
                    }
                    score = move + 0.35f * std::abs (mean - (float) centre);
                }
                if (v.front() < centre - 14 || v.back() > centre + 20)
                    score += 50.0f;
                if (score < bestScore)
                {
                    bestScore = score;
                    best = v;
                }
            }
        }

        if (best.empty()) // no voicing could put that note on top: fall back to the automatic choice
            return make (root, quality, includeBass, centre, previousUpper, bassLow, style, 0);

        // Spread the voicing: drop 2 moves the second-highest note down an octave, open also drops the fourth
        if (style == drop2 || style == open)
        {
            std::sort (best.begin(), best.end());
            if (best.size() >= 3) best[best.size() - 2] -= 12;
            if (style == open && best.size() >= 4) best[best.size() - 4] -= 12;
            std::sort (best.begin(), best.end());
        }

        std::vector<int> notes;
        if (includeBass)
        {
            const int bass = bassLow + root % 12; // C2 .. B2 by default
            notes.push_back (bass);
            // A fifth above the bass fills out chords that have a natural fifth
            if (q.naturalFifth && std::find (pcs.begin(), pcs.end(), (root + 7) % 12) == pcs.end())
                notes.push_back (bass + 7);
        }
        notes.insert (notes.end(), best.begin(), best.end());
        return notes;
    }

    juce::String name (int root, int quality, int bassPc)
    {
        auto n = ChordQ::noteName (root) + ChordQ::info (quality).suffix;
        if (bassPc >= 0 && bassPc % 12 != root % 12)
            n += "/" + ChordQ::noteName (bassPc);
        return n;
    }

    Chord identify (const std::vector<int>& notesIn)
    {
        Chord c;
        if (notesIn.empty())
            return c;
        auto notes = notesIn;
        std::sort (notes.begin(), notes.end());
        c.notes = notes;
        std::set<int> pcs;
        for (int n : notes) pcs.insert (n % 12);
        const int bass = notes.front() % 12;

        float bestScore = -1.0e9f;
        for (int r : pcs)
        {
            std::set<int> rel;
            for (int p : pcs) rel.insert ((p - r + 12) % 12);
            for (int q = 0; q < ChordQ::numQualities; ++q)
            {
                const auto& info = ChordQ::info (q);
                std::set<int> want { 0 };
                for (int iv : info.upper) want.insert (iv % 12);
                if (info.naturalFifth) want.insert (7);
                int matched = 0, missing = 0, extra = 0;
                for (int w : want)
                    if (rel.count (w)) ++matched;
                    else if (w != 7) ++missing;
                for (int x : rel)
                    if (! want.count (x)) ++extra;
                const float score = 2.0f * (float) matched - 1.5f * (float) missing - 2.0f * (float) extra
                                  + (r == bass ? 1.5f : 0.0f) - 0.05f * (float) want.size();
                if (score > bestScore)
                {
                    bestScore = score;
                    c.root = r;
                    c.quality = q;
                }
            }
        }

        if (pcs.size() < 3 || bestScore < 2.0f)
        {
            // Not a recognisable chord: name it by its notes
            juce::StringArray parts;
            for (int p : pcs) parts.add (ChordQ::noteName (p));
            c.name = parts.joinIntoString ("-");
            c.root = -1;
            c.quality = -1;
        }
        else
            c.name = name (c.root, c.quality, bass);
        return c;
    }
}

namespace Voicing
{
    Chord parseName (const juce::String& nameIn)
    {
        Chord c;
        auto text = nameIn.trim().replace (juce::CharPointer_UTF8 ("\xe2\x99\xad"), "b").replace (juce::CharPointer_UTF8 ("\xe2\x99\xaf"), "#");
        if (text.isEmpty()) return c;
        const auto letter = juce::CharacterFunctions::toUpperCase (text[0]);
        const int letterPc = letter == 'C' ? 0 : letter == 'D' ? 2 : letter == 'E' ? 4 : letter == 'F' ? 5 : letter == 'G' ? 7 : letter == 'A' ? 9 : letter == 'B' ? 11 : -1;
        if (letterPc < 0) return c;
        int root = letterPc, i = 1;
        if (i < text.length() && (text[i] == '#' || text[i] == 'b'))  // one sharp or flat right after the letter
        {
            root += text[i] == '#' ? 1 : -1;
            ++i;
        }
        root = (root % 12 + 12) % 12;
        auto suffix = text.substring (i).upToFirstOccurrenceOf ("/", false, false); // ignore slash bass
        suffix = suffix.replace (juce::CharPointer_UTF8 ("\xce\x94"), "maj").replace (juce::CharPointer_UTF8 ("\xc2\xb0"), "dim")
                       .replace (juce::CharPointer_UTF8 ("\xc3\xb8"), "m7b5").removeCharacters ("() ").toLowerCase();

        static const std::vector<std::pair<const char*, int>> aliases {
            { "", ChordQ::add9 }, { "maj", ChordQ::M7 }, { "maj7", ChordQ::M7 }, { "ma7", ChordQ::M7 }, 
            { "maj9", ChordQ::M9 }, { "ma9", ChordQ::M9 }, { "maj9#11", ChordQ::M9s11 }, { "maj7#11", ChordQ::M9s11 }, { "lyd", ChordQ::M9s11 },
            { "6/9", ChordQ::M69 }, { "69", ChordQ::M69 }, { "6", ChordQ::M69 }, { "add9", ChordQ::add9 }, { "add2", ChordQ::add9 },
            { "maj7#5", ChordQ::M7s5 }, { "aug", ChordQ::M7s5 }, { "+", ChordQ::M7s5 },
            { "m", ChordQ::m7 }, { "min", ChordQ::m7 }, { "-", ChordQ::m7 }, { "m7", ChordQ::m7 }, { "min7", ChordQ::m7 }, { "-7", ChordQ::m7 },
            { "m9", ChordQ::m9 }, { "min9", ChordQ::m9 }, { "-9", ChordQ::m9 }, { "m11", ChordQ::m11 }, { "min11", ChordQ::m11 }, { "-11", ChordQ::m11 },
            { "m6/9", ChordQ::m69 }, { "m69", ChordQ::m69 }, { "m6", ChordQ::m69 }, { "mmaj9", ChordQ::mM9 }, { "mmaj7", ChordQ::mM9 }, { "mm9", ChordQ::mM9 },
            { "7", ChordQ::d7 }, { "dom7", ChordQ::d7 }, { "9", ChordQ::d9 }, { "13", ChordQ::d13 }, { "7#9", ChordQ::d7s9 }, { "7+9", ChordQ::d7s9 },
            { "13b9", ChordQ::d13b9 }, { "7b9", ChordQ::d13b9 }, { "7alt", ChordQ::d7alt }, { "alt", ChordQ::d7alt }, { "9#11", ChordQ::d9s11 },
            { "7#11", ChordQ::d9s11 }, { "13#11", ChordQ::d9s11 }, { "11", ChordQ::d11 },
            { "7sus4", ChordQ::sus7 }, { "7sus", ChordQ::sus7 }, { "sus", ChordQ::sus9 }, { "sus4", ChordQ::sus9 }, { "sus2", ChordQ::sus9 },
            { "9sus4", ChordQ::sus9 }, { "9sus", ChordQ::sus9 }, { "13sus4", ChordQ::sus13 }, { "13sus", ChordQ::sus13 },
            { "m7b5", ChordQ::m7b5 }, { "min7b5", ChordQ::m7b5 }, { "-7b5", ChordQ::m7b5 }, { "m9b5", ChordQ::h9 }, { "m11b5", ChordQ::m11b5 },
            { "dim", ChordQ::dim7 }, { "dim7", ChordQ::dim7 }, { "o", ChordQ::dim7 }, { "o7", ChordQ::dim7 },
        };
        for (const auto& [alias, q] : aliases)
            if (suffix == alias)
            {
                c.root = root;
                c.quality = q;
                c.notes = make (root, q, true, 62, nullptr);
                c.name = name (root, q);
                return c;
            }
        // Also accept our own display names exactly ("maj9(#11)" etc. without the brackets)
        for (int q = 0; q < ChordQ::numQualities; ++q)
            if (juce::String (ChordQ::info (q).suffix).removeCharacters ("() ").toLowerCase() == suffix)
            {
                c.root = root;
                c.quality = q;
                c.notes = make (root, q, true, 62, nullptr);
                c.name = name (root, q);
                return c;
            }
        return c;
    }
}

namespace ChordPresets
{
    using namespace ChordQ;

    struct Palette
    {
        const char* name;
        int tonic;
        std::array<std::pair<int, int>, 7> chords; // (semitones above tonic, quality) for C D E F G A B
        const char* flavours;                       // C# D# F# G# A#
    };

    // Original progressions in a neo soul / R&B style: extended minors, sus dominants,
    // gospel passing chords and chromatic slides.
    const std::vector<Palette>& palettes()
    {
        static const std::vector<Palette> list {
            { "Velvet Sunday",        3,  {{ {0,M9}, {2,m9}, {4,m7}, {5,M9s11}, {7,sus13}, {9,m11}, {10,d9} }}, "SDTSC" },
            { "Smoke & Honey",        10, {{ {0,m11}, {3,M9}, {5,m9}, {7,d7s9}, {8,M9s11}, {10,sus9}, {2,h9} }}, "TSCDS" },
            { "Late Night Groove",    2,  {{ {0,m9}, {5,d13}, {10,M9}, {3,M9s11}, {7,m11}, {8,d9s11}, {9,d7s9} }}, "SCTSU" },
            { "Golden Hour Gospel",   8,  {{ {0,M69}, {5,M9}, {9,m9}, {2,m11}, {7,sus13}, {4,m7}, {7,d13b9} }}, "DSDSU" },
            { "Rainy Window",         9,  {{ {0,m9}, {8,M9s11}, {5,M9}, {3,h9}, {7,d7alt}, {10,M7s5}, {2,m11} }}, "CTSDC" },
            { "Soulful Strut",        5,  {{ {0,m11}, {10,d9}, {3,M9}, {8,M9s11}, {7,d7s9}, {5,m9}, {0,mM9} }}, "SCSTC" },
            { "Sunday Choir",         1,  {{ {0,M9}, {5,M69}, {6,dim7}, {10,m9}, {3,m11}, {7,sus13}, {8,M9} }}, "DSDUS" },
            { "Midnight Drive",       4,  {{ {0,m9}, {3,M9s11}, {5,m11}, {10,d13}, {8,M9}, {1,h9}, {6,d7alt} }}, "TCSTS" },
            { "Silk Sheets",          6,  {{ {0,M9}, {11,M9s11}, {4,m9}, {9,d13}, {2,M9}, {8,m11}, {1,sus13} }}, "CSTCU" },
            { "Cocoa Butter",         11, {{ {0,m9}, {5,d9}, {10,m11}, {3,M9}, {8,M69}, {1,h9}, {6,d13b9} }}, "SSCTS" },
            { "Front Porch Hymn",     7,  {{ {0,M69}, {4,m7}, {5,M9}, {6,dim7}, {7,add9}, {9,m9}, {2,sus9} }}, "DSDUS" },
            { "Neon Lounge",          0,  {{ {0,m9}, {2,h9}, {3,M9}, {5,m11}, {7,d7alt}, {8,M9s11}, {10,d13} }}, "TSCTS" },
            { "Hazy Morning",         5,  {{ {0,M9}, {2,m11}, {4,m9}, {7,sus13}, {9,m7}, {10,M9s11}, {7,d13} }}, "CSDCS" },
            { "Bedroom Session",      3,  {{ {0,m9}, {5,d9}, {8,M9}, {7,d7s9}, {10,sus9}, {1,M9s11}, {2,h9} }}, "SCTSD" },
            { "Liquid Groove",        9,  {{ {0,m11}, {2,m9}, {5,M9}, {7,d13}, {3,h9}, {8,d7s9}, {10,M9s11} }}, "CCSTS" },
            { "Church Clap",          10, {{ {0,M9}, {4,dim7}, {5,M69}, {9,m9}, {2,m11}, {7,d13b9}, {7,sus13} }}, "DSDSU" },
            { "Velour Slow Jam",      8,  {{ {0,M9}, {7,m11}, {5,M9s11}, {2,m9}, {10,sus13}, {3,m9}, {1,M9} }}, "SCTSC" },
            { "Coffee Shop Poet",     2,  {{ {0,M9}, {4,m9}, {9,m11}, {5,M9s11}, {7,d9}, {11,m7}, {10,d13} }}, "SDSCT" },
            { "Burgundy",             1,  {{ {0,m11}, {3,m9}, {8,M9}, {5,M9s11}, {10,d7s9}, {6,M69}, {7,h9} }}, "TSCSD" },
            { "Slow Burn",            7,  {{ {0,m9}, {5,m11}, {10,d13}, {3,M9}, {8,M7s5}, {2,d7alt}, {0,mM9} }}, "SCTSS" },
            { "Lavender Haze",        4,  {{ {0,M9s11}, {2,M9}, {9,m9}, {11,m11}, {4,m7}, {5,M9}, {7,sus13} }}, "CTCSU" },
            { "Brownstone Steps",     5,  {{ {0,m9}, {3,d13}, {8,M9}, {1,M9s11}, {7,d7s9}, {10,m11}, {5,sus9} }}, "STCST" },
            { "Altar Call",           3,  {{ {0,M69}, {5,M9}, {7,sus13}, {9,m9}, {4,dim7}, {2,m11}, {7,d13b9} }}, "DUSDS" },
            { "Moonlit Boulevard",    11, {{ {0,M9}, {3,m11}, {6,d13}, {8,m9}, {10,M9s11}, {1,sus13}, {4,m7} }}, "SCTSC" },
            { "Velvet Rope",          6,  {{ {0,m9}, {2,M9s11}, {5,m11}, {7,d7alt}, {9,M9}, {10,d9s11}, {3,h9} }}, "TSCTS" },
            { "Honey Drip",           10, {{ {0,M9}, {9,m11}, {2,m9}, {7,d13}, {5,M9s11}, {4,h9}, {3,m69} }}, "SCSTD" },
            { "Sanctified Groove",    8,  {{ {0,d9}, {5,d9}, {7,d7s9}, {3,m11}, {10,sus13}, {0,d13}, {6,M9} }}, "CSTCS" },
            { "Quiet Storm",          2,  {{ {0,m9}, {10,M9}, {8,M9s11}, {7,d13b9}, {5,m11}, {3,M9}, {1,d7alt} }}, "SCDTS" },
            { "Stoop Kids",           0,  {{ {0,M9}, {9,m9}, {2,m11}, {7,d9}, {4,m7}, {5,M69}, {8,d9s11} }}, "SDSCT" },
            { "After Hours Bar",      9,  {{ {0,m69}, {5,m9}, {7,d7alt}, {10,M9}, {3,h9}, {8,M9s11}, {2,d13} }}, "TSCSS" },
            { "Mahogany",             3,  {{ {0,m11}, {8,M9}, {10,d13}, {5,m9}, {1,M9s11}, {7,d7s9}, {3,M7s5} }}, "SCTSC" },
            { "Soft Landing",         5,  {{ {0,add9}, {5,M9}, {9,m11}, {2,m9}, {7,sus9}, {4,m7}, {10,M69} }}, "SUDSC" },
            { "Crushed Velvet",       6,  {{ {0,m9}, {10,m11}, {8,M9s11}, {1,d13}, {3,M9}, {5,d7s9}, {11,sus13} }}, "CTSCS" },
            { "Uptown Prayer",        1,  {{ {0,M9}, {6,dim7}, {5,m9}, {10,m11}, {3,m9}, {8,sus13}, {8,d13b9} }}, "DSDSU" },
            { "Rooftop Summer",       7,  {{ {0,M9}, {2,m9}, {5,M9s11}, {4,m11}, {9,m9}, {10,d9}, {7,M69} }}, "CSTSD" },
            { "Dimmer Switch",        4,  {{ {0,m9}, {7,d7alt}, {5,M9}, {3,h9}, {8,d7s9}, {1,m11}, {10,d9s11} }}, "TCSST" },
            { "Sweet Tea",            10, {{ {0,M69}, {2,m11}, {5,M9}, {9,m7}, {7,d13}, {4,h9}, {3,M9s11} }}, "SDCSU" },
            { "Indigo",               11, {{ {0,m11}, {5,d13}, {10,M9}, {3,M9s11}, {8,m9}, {1,h9}, {6,d7s9} }}, "SCTSS" },
            { "Saturday Fish Fry",    5,  {{ {0,d9}, {5,d13}, {7,d7s9}, {10,d9}, {3,d13}, {8,M9}, {2,m11} }}, "CSTCS" },
            { "Candlelight",          8,  {{ {0,M9}, {10,m11}, {3,m9}, {5,M9s11}, {7,sus13}, {1,M9}, {9,dim7} }}, "SCTDU" },
            { "Blue Note Stoop",      2,  {{ {0,m69}, {2,h9}, {7,d7alt}, {5,M9}, {10,M9s11}, {3,d13}, {0,mM9} }}, "TSCTS" },
            { "Champagne Room",       3,  {{ {0,M9s11}, {7,m9}, {2,m11}, {5,M9}, {10,d13}, {8,M69}, {1,d9s11} }}, "CTSCS" },
            { "Grandma's Piano",      7,  {{ {0,M69}, {5,add9}, {7,sus9}, {9,m7}, {4,m9}, {2,m11}, {6,dim7} }}, "DUSDS" },
            { "Fade to Grey",         9,  {{ {0,m11}, {3,M9s11}, {5,m9}, {10,d13}, {8,M7s5}, {7,d7alt}, {2,h9} }}, "SCTSD" },
            { "Cruise Control",       0,  {{ {0,M9}, {4,m11}, {9,m9}, {2,d13}, {5,M9s11}, {7,sus13}, {10,M9} }}, "SCSTC" },
            { "Plum Wine",            1,  {{ {0,m9}, {8,M9}, {6,d13}, {11,M9s11}, {3,m11}, {10,d7s9}, {5,h9} }}, "TSCSS" },
            { "Testimony",            10, {{ {0,M9}, {5,M69}, {6,dim7}, {7,m11}, {2,m9}, {9,d13b9}, {4,sus13} }}, "DSDSU" },
            { "Lo-Fi Lover",          4,  {{ {0,M9}, {9,m9}, {5,M9s11}, {7,d9}, {2,m11}, {11,d7s9}, {3,M7s5} }}, "CSTSC" },
            { "Velvet Underpass",     6,  {{ {0,m11}, {5,m9}, {10,d7alt}, {3,M9}, {8,M9s11}, {1,d13}, {11,h9} }}, "STCST" },
            { "Last Call",            8,  {{ {0,M9}, {2,m11}, {7,sus13}, {7,d13b9}, {9,m9}, {5,M9s11}, {4,M69} }}, "SDSCU" },
        };
        return list;
    }

    int count() { return (int) palettes().size(); }

    void fillBlackKeys (ChordSet& set, const char* flavours)
    {
        // Each black key leads into the white key just above it
        static constexpr std::array<int, 5> targetWhite { 1, 2, 4, 5, 6 };
        for (size_t i = 0; i < blackPcs.size(); ++i)
        {
            const auto& target = set.chords[(size_t) whitePcs[(size_t) targetWhite[i]]];
            const int tRoot = target.root >= 0 ? target.root : (target.notes.empty() ? 0 : target.notes.front() % 12);
            const int tQual = target.quality >= 0 ? target.quality : ChordQ::m9;
            int root = 0, quality = d13b9;
            switch (flavours != nullptr && std::strlen (flavours) > i ? flavours[i] : 'S')
            {
                case 'T': root = (tRoot + 1) % 12;  quality = d9s11; break;
                case 'C': root = (tRoot + 1) % 12;  quality = tQual; break;
                case 'D': root = (tRoot + 11) % 12; quality = dim7;  break;
                case 'U': root = (tRoot + 7) % 12;  quality = sus13; break;
                case 'S':
                default:  root = (tRoot + 7) % 12;  quality = d13b9; break;
            }
            auto& c = set.chords[(size_t) blackPcs[i]];
            c.root = root;
            c.quality = quality;
            c.notes = Voicing::make (root, quality, true, 62, nullptr);
            c.name = Voicing::name (root, quality);
        }
    }

    ChordSet get (int index)
    {
        const auto& p = palettes()[(size_t) juce::jlimit (0, count() - 1, index)];
        ChordSet set;
        set.name = juce::String (index + 1).paddedLeft ('0', 2) + "  " + p.name + "  (" + ChordQ::noteName (p.tonic) + ")";
        set.tonic = p.tonic;
        for (size_t i = 0; i < 7; ++i)
        {
            auto& c = set.chords[(size_t) whitePcs[i]];
            c.root = (p.tonic + p.chords[i].first) % 12;
            c.quality = p.chords[i].second;
            c.notes = Voicing::make (c.root, c.quality, true, 62, nullptr);
            c.name = Voicing::name (c.root, c.quality);
        }
        fillBlackKeys (set, p.flavours);
        return set;
    }
}

namespace ScaleChords
{
    static const std::array<std::array<int, 7>, 8> scales {{
        { 0, 2, 4, 5, 7, 9, 11 },   // Major
        { 0, 2, 3, 5, 7, 8, 10 },   // Natural Minor
        { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
        { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydian
        { 0, 2, 4, 6, 7, 9, 11 },   // Lydian
        { 0, 1, 3, 5, 7, 8, 10 },   // Phrygian
        { 0, 2, 3, 5, 7, 8, 11 },   // Harmonic Minor
        { 0, 2, 3, 5, 7, 9, 11 },   // Melodic Minor
    }};

    juce::StringArray scaleNames()
    {
        return { "Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Phrygian", "Harmonic Minor", "Melodic Minor" };
    }

    // Picks a lush (9th-level) quality from the 3rd, 5th, 7th and 9th the scale gives this degree
    static int qualityFor (int third, int fifth, int seventh, int ninth)
    {
        using namespace ChordQ;
        if (third == 4 && fifth == 7 && seventh == 11) return ninth == 2 ? M9 : M7;
        if (third == 4 && fifth == 8 && seventh == 11) return M7s5;
        if (third == 3 && fifth == 7 && seventh == 10) return ninth == 2 ? m9 : m7;
        if (third == 3 && fifth == 7 && seventh == 11) return ninth == 2 ? mM9 : m7;
        if (third == 4 && fifth == 7 && seventh == 10) return ninth == 2 ? d9 : ninth == 1 ? d13b9 : d7s9;
        if (third == 3 && fifth == 6 && seventh == 10) return ninth == 2 ? h9 : m7b5;
        if (third == 3 && fifth == 6 && seventh == 9)  return dim7;
        if (third == 4 && fifth == 6)                  return d9s11;
        return third == 4 ? M9 : m9;
    }

    ChordSet make (int keyPc, int scaleIndex)
    {
        const auto& sc = scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, scaleIndex)];
        ChordSet set;
        set.tonic = keyPc;
        set.name = ChordQ::noteName (keyPc) + " " + scaleNames()[scaleIndex] + " (scale chords)";
        auto degree = [&] (int d) { return sc[(size_t) (d % 7)] + 12 * (d / 7); };
        for (int d = 0; d < 7; ++d)
        {
            const int r = degree (d);
            auto& c = set.chords[(size_t) ChordPresets::whitePcs[(size_t) d]];
            c.root = (keyPc + r) % 12;
            c.quality = qualityFor (degree (d + 2) - r, degree (d + 4) - r, degree (d + 6) - r, (degree (d + 8) - r) % 12);
            c.notes = Voicing::make (c.root, c.quality, true, 62, nullptr);
            c.name = Voicing::name (c.root, c.quality);
        }
        ChordPresets::fillBlackKeys (set, "STCSD");
        return set;
    }
}

ChordSet transposeSet (const ChordSet& set, int semitones)
{
    semitones = ((semitones % 12) + 12) % 12;
    if (semitones > 6) semitones -= 12; // move the shortest way
    if (semitones == 0) return set;
    ChordSet out = set;
    if (set.tonic >= 0) out.tonic = (set.tonic + semitones + 12) % 12;
    for (auto& c : out.chords)
    {
        if (c.root >= 0 && c.quality >= 0)
        {
            c.root = (c.root + semitones + 12) % 12;
            c.notes = Voicing::make (c.root, c.quality, true, 62, nullptr);
            c.name = Voicing::name (c.root, c.quality);
        }
        else
        {
            for (auto& n : c.notes) n += semitones;
            c = [&] { auto id = Voicing::identify (c.notes); id.notes = c.notes; return id; }();
        }
    }
    return out;
}

namespace Suggest
{
    std::array<float, 12> score (const Chord& last, const std::array<Chord, 12>& options)
    {
        std::array<float, 12> out {};
        out.fill (-100.0f);
        if (last.root < 0 && last.notes.empty())
            return out;
        const int lastRoot = last.root >= 0 ? last.root : last.notes.front() % 12;
        const bool lastDominant = last.quality >= 0 && ChordQ::info (last.quality).family == ChordQ::dominant;
        std::set<int> lastPcs;
        for (int n : last.notes) lastPcs.insert (n % 12);

        // How good each root movement usually sounds in soul / R&B / gospel
        static const std::array<float, 12> motion { -5.0f, 0.6f, 1.0f, 0.8f, 0.7f, 3.0f, 0.3f, 1.0f, 0.9f, 1.2f, 1.2f, 0.8f };
        for (size_t k = 0; k < 12; ++k)
        {
            const auto& o = options[k];
            if (o.isEmpty() || o.name == last.name)
                continue;
            const int root = o.root >= 0 ? o.root : o.notes.front() % 12;
            const int step = (root - lastRoot + 12) % 12;
            float s = motion[(size_t) step];
            if (lastDominant && step == 5) s += 1.5f;   // dominant resolving up a fourth
            if (lastDominant && step == 11) s += 1.2f;  // tritone sub resolving down a half step
            int common = 0;
            for (int n : o.notes)
                if (lastPcs.count (n % 12)) ++common;
            s += 0.35f * (float) std::min (common, 4);
            out[k] = s;
        }
        return out;
    }
}
