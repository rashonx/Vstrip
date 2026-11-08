#include "PresetFile.h"

namespace PresetFile
{
    juce::String extension() { return ".soulkeys"; }

    juce::File folder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("Dodge City Plugins").getChildFile ("Soul Keys").getChildFile ("Presets");
        dir.createDirectory();
        return dir;
    }

    static const std::array<const char*, 12> keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String toJson (const ChordSet& set)
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "Rashon Soul Keys preset");
        root->setProperty ("version", 1);
        root->setProperty ("name", set.name);
        if (set.tonic >= 0) root->setProperty ("tonic", ChordQ::noteName (set.tonic));
        juce::Array<juce::var> chords;
        for (size_t pc = 0; pc < 12; ++pc)
        {
            const auto& c = set.chords[pc];
            auto* o = new juce::DynamicObject();
            o->setProperty ("key", keyNames[pc]);
            o->setProperty ("chord", c.name);
            juce::Array<juce::var> notes;
            for (int n : c.notes) notes.add (n);
            o->setProperty ("notes", notes);
            chords.add (juce::var (o));
        }
        root->setProperty ("chords", chords);
        return juce::JSON::toString (juce::var (root), false);
    }

    static Chord chordFromVar (const juce::var& v)
    {
        if (v.isString())
            return Voicing::parseName (v.toString());
        Chord c = Voicing::parseName (v.getProperty ("chord", v.getProperty ("name", "")).toString());
        if (c.isEmpty())
        {
            // No readable name: use the notes as written
            std::vector<int> notes;
            if (auto* arr = v.getProperty ("notes", {}).getArray())
                for (auto& n : *arr) notes.push_back ((int) n);
            if (notes.size() >= 3)
            {
                c = Voicing::identify (notes);
                const auto given = v.getProperty ("chord", "").toString();
                if (given.isNotEmpty()) c.name = given;
            }
        }
        return c;
    }

    bool fromJson (const juce::String& json, ChordSet& out, juce::String& error)
    {
        const auto v = juce::JSON::parse (json);
        if (! v.isObject())
        {
            error = "This isn't a Soul Keys preset (couldn't read it as JSON)";
            return false;
        }
        out = ChordSet();
        out.name = v.getProperty ("name", "Imported preset").toString();
        const auto tonic = Voicing::parseName (v.getProperty ("tonic", "").toString() + "maj7");
        out.tonic = tonic.root;

        bool blackGiven = false;
        if (auto* chords = v.getProperty ("chords", {}).getArray())
        {
            for (int i = 0; i < chords->size(); ++i)
            {
                const auto& item = chords->getReference (i);
                int pc = i;
                const auto key = item.isObject() ? item.getProperty ("key", "").toString().trim() : juce::String();
                for (size_t k = 0; k < 12; ++k)
                    if (key.equalsIgnoreCase (keyNames[k])) pc = (int) k;
                if (key.equalsIgnoreCase ("Db")) pc = 1;
                if (key.equalsIgnoreCase ("Eb")) pc = 3;
                if (key.equalsIgnoreCase ("Gb")) pc = 6;
                if (key.equalsIgnoreCase ("Ab")) pc = 8;
                if (key.equalsIgnoreCase ("Bb")) pc = 10;
                if (pc < 0 || pc > 11) continue;
                out.chords[(size_t) pc] = chordFromVar (item);
                if (std::find (ChordPresets::blackPcs.begin(), ChordPresets::blackPcs.end(), pc) != ChordPresets::blackPcs.end() && ! out.chords[(size_t) pc].isEmpty())
                    blackGiven = true;
            }
        }
        else
        {
            if (auto* white = v.getProperty ("white", {}).getArray())
                for (int i = 0; i < std::min (7, white->size()); ++i)
                    out.chords[(size_t) ChordPresets::whitePcs[(size_t) i]] = chordFromVar (white->getReference (i));
            if (auto* black = v.getProperty ("black", {}).getArray())
                for (int i = 0; i < std::min (5, black->size()); ++i)
                {
                    out.chords[(size_t) ChordPresets::blackPcs[(size_t) i]] = chordFromVar (black->getReference (i));
                    blackGiven = blackGiven || ! out.chords[(size_t) ChordPresets::blackPcs[(size_t) i]].isEmpty();
                }
        }

        // White keys must all be filled
        juce::StringArray missing;
        for (size_t i = 0; i < 7; ++i)
            if (out.chords[(size_t) ChordPresets::whitePcs[i]].isEmpty())
                missing.add (keyNames[(size_t) ChordPresets::whitePcs[i]]);
        if (! missing.isEmpty())
        {
            error = "The preset is missing readable chords for key(s) " + missing.joinIntoString (", ");
            return false;
        }
        // Any empty black key gets a related chord
        ChordSet related = out;
        ChordPresets::fillBlackKeys (related, v.getProperty ("flavours", "STCSD").toString().toRawUTF8());
        for (int pc : ChordPresets::blackPcs)
            if (out.chords[(size_t) pc].isEmpty())
                out.chords[(size_t) pc] = related.chords[(size_t) pc];
        if (out.tonic < 0)
            out.tonic = out.chords[0].root;
        juce::ignoreUnused (blackGiven);
        return true;
    }

    bool load (const juce::File& file, ChordSet& out, juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = "File not found";
            return false;
        }
        return fromJson (file.loadFileAsString(), out, error);
    }

    juce::File save (const ChordSet& set, juce::String& error)
    {
        auto name = juce::File::createLegalFileName (set.name.trim().isEmpty() ? "My preset" : set.name.trim());
        auto file = folder().getChildFile (name + extension());
        if (! file.replaceWithText (toJson (set)))
        {
            error = "Couldn't write " + file.getFullPathName();
            return {};
        }
        return file;
    }
}
