// Plays a synthetic vocal and beat through the plugin, then saves a picture of the window.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <thread>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double rate = 48000.0;
    const int block = 480;
    VocalSitterProcessor proc;
    proc.setRateAndBufferSizeDetails (rate, block);
    proc.prepareToPlay (rate, block);
    for (auto [id, v] : { std::pair { "amount", 0.8f }, { "release", 0.5f } })
        proc.apvts.getParameter (id)->setValueNotifyingHost (v);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setVisible (true);

    juce::AudioBuffer<float> buf (4, block);
    juce::MidiBuffer midi;
    juce::Random rng (7);
    float pinkState[3] = {};
    const auto twoPi = juce::MathConstants<double>::twoPi;
    long t = 0;

    // Audio runs on its own thread while the window's timer keeps redrawing, like in a DAW
    std::thread audio ([&]
    {
    for (int frame = 0; frame < 90; ++frame)
    {
        for (int b = 0; b < 4; ++b, t += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const double s = (double) (t + i) / rate;
                // Vocal: sung note with vibrato and vowel-like formants
                const double f0 = 220.0 * (1.0 + 0.01 * std::sin (twoPi * 5.0 * s));
                double v = 0.0;
                for (int h = 1; h <= 30; ++h)
                {
                    const double hz = f0 * h;
                    const double formant = std::exp (-std::pow (std::log (hz / 700.0), 2) * 6) + 0.7 * std::exp (-std::pow (std::log (hz / 1200.0), 2) * 6)
                                         + 0.4 * std::exp (-std::pow (std::log (hz / 2700.0), 2) * 5) + 0.02;
                    v += formant * std::sin (twoPi * hz * s) / h;
                }
                // Beat: kick, bass and pinkish noise
                const double kickT = std::fmod (s, 0.5);
                const double kick = std::exp (-kickT * 18.0) * std::sin (twoPi * (50.0 + 80.0 * std::exp (-kickT * 30.0)) * kickT);
                const float white = rng.nextFloat() * 2.0f - 1.0f;
                pinkState[0] = 0.997f * pinkState[0] + 0.029591f * white;
                pinkState[1] = 0.985f * pinkState[1] + 0.032534f * white;
                pinkState[2] = 0.950f * pinkState[2] + 0.048056f * white;
                const float pink = pinkState[0] + pinkState[1] + pinkState[2] + 0.1848f * white;
                const double beat = 0.5 * kick + 0.12 * std::sin (twoPi * 55.0 * s) + 0.35 * pink
                                  + 0.05 * (std::sin (twoPi * 330.0 * s) + std::sin (twoPi * 415.0 * s) + std::sin (twoPi * 494.0 * s));
                for (int ch = 0; ch < 2; ++ch)
                {
                    buf.setSample (ch, i, (float) beat);
                    buf.setSample (ch + 2, i, (float) (0.25 * v));
                }
            }
            proc.processBlock (buf, midi);
        }
        juce::Thread::sleep (34);
    }
    juce::MessageManager::callAsync ([] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
    });
    juce::MessageManager::getInstance()->runDispatchLoop();
    audio.join();

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File out (argc > 1 ? argv[1] : "screenshot.png");
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::printf ("saved %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    return 0;
}
