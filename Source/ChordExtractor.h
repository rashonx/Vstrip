#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Chords.h"

// Pulls the chords out of a MIDI file or an audio file and lays them across the 12 keys:
// the first 7 distinct chords go on the white keys, the next 5 on the black keys.
namespace ChordExtractor
{
    struct Result
    {
        ChordSet set;
        int chordsFound = 0;
        juce::String error;
    };

    Result fromFile (const juce::File& file);
    Result fromMidi (const juce::MidiFile& midi, const juce::String& name);
    Result fromAudio (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name);

    bool isMidiFile (const juce::File& f);
    juce::String supportedPatterns();
}
