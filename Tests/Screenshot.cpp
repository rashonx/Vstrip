// Plays a chord through the plugin and saves a picture of the window.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <thread>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    SoulKeysProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    auto set = [&] (const char* id, float v) { auto* q = proc.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    set ("split", 1.0f);
    set ("chordSetB", 18.0f);
    set ("extensionsB", 2.0f);
    proc.applySoundPreset (1, 7);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    if (argc > 2)
        if (auto* e = dynamic_cast<SoulKeysEditor*> (editor.get()))
        {
            e->getView().setSkin (juce::String (argv[2]).getIntValue());
            if (argc > 3) e->setScale ((float) juce::String (argv[3]).getDoubleValue());
        }
    editor->setVisible (true);

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 60; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 2) { midi.addEvent (juce::MidiMessage::noteOn (1, 50, 0.8f), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 0); }
            proc.processBlock (buf, midi);
            juce::Thread::sleep (10);
        }
        juce::Thread::sleep (300);
        juce::MessageManager::callAsync ([] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
    });
    juce::MessageManager::getInstance()->runDispatchLoop();
    audio.join();

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File out (argc > 1 ? argv[1] : "screenshot.png");
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::printf ("saved %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
