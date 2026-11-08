#pragma once

#include <juce_core/juce_core.h>
#include "Chords.h"

// ".soulkeys" chord preset files (JSON). Two ways to write the chords:
//   simple:  { "name": "...", "white": [7 chord names for C D E F G A B], "black": [5 names for C# D# F# G# A#] }
//            (black is optional - related chords are added automatically)
//   full:    { "name": "...", "chords": [ { "key": "C", "chord": "Ebmaj9", "notes": [...] }, ... ] }
namespace PresetFile
{
    juce::File folder();                                   // Documents/Dodge City Plugins/Soul Keys/Presets
    juce::String toJson (const ChordSet& set);
    bool fromJson (const juce::String& json, ChordSet& out, juce::String& error);
    bool load (const juce::File& file, ChordSet& out, juce::String& error);
    juce::File save (const ChordSet& set, juce::String& error); // writes into folder(), returns the file
    juce::String extension();                              // ".soulkeys"
}
