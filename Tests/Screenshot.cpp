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
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setVisible (true);

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 60; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 2) midi.addEvent (juce::MidiMessage::noteOn (1, 62, 0.8f), 0);
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
