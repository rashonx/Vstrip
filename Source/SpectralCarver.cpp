#include "SpectralCarver.h"

void SpectralCarver::prepare (double sampleRate, int maxBeatChannels)
{
    sr = sampleRate;

    // Square-root Hann on both analysis and synthesis; at 75% overlap the frames sum to a constant
    window.resize (fftSize);
    float windowSum = 0.0f;
    for (int n = 0; n < fftSize; ++n)
    {
        window[(size_t) n] = std::sqrt (0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) fftSize)));
        windowSum += window[(size_t) n];
    }
    refDb = juce::Decibels::gainToDecibels (windowSum * 0.5f);

    vocalFifo.assign (fftSize, 0.0f);
    frame.assign (fftSize * 2, 0.0f);
    vocalPower.assign (numBins, 0.0f);
    beatPowerIn.assign (numBins, 0.0f);
    beatPowerOut.assign (numBins, 0.0f);
    binGain.assign (numBins, 1.0f);
    inFifo.assign ((size_t) juce::jmax (1, maxBeatChannels), std::vector<float> (fftSize, 0.0f));
    outFifo.assign ((size_t) juce::jmax (1, maxBeatChannels), std::vector<float> (fftSize, 0.0f));

    // Log-spaced bands from 20 Hz up to 20 kHz (or Nyquist)
    const float binHz = (float) sr / (float) fftSize;
    const float top = juce::jmin (20000.0f, (float) sr * 0.5f);
    for (int b = 0; b <= numBands; ++b)
    {
        const float hz = 20.0f * std::pow (top / 20.0f, (float) b / (float) numBands);
        bandEdges[(size_t) b] = juce::jlimit (1, numBins - 1, juce::roundToInt (hz / binHz));
    }
    for (int b = 0; b < numBands; ++b)
    {
        bandEdges[(size_t) b + 1] = juce::jmax (bandEdges[(size_t) b + 1], bandEdges[(size_t) b] + 1);
        bandCentreHz[(size_t) b] = std::sqrt ((float) bandEdges[(size_t) b] * (float) bandEdges[(size_t) b + 1]) * binHz;
    }

    // For each bin, which two band centres it sits between (for smooth gain curves)
    binBand.assign (numBins, 0);
    binFrac.assign (numBins, 0.0f);
    for (int k = 0; k < numBins; ++k)
    {
        const float hz = juce::jmax (1.0f, (float) k * binHz);
        int b = 0;
        while (b < numBands - 2 && bandCentreHz[(size_t) b + 1] < hz)
            ++b;
        const float lo = std::log (bandCentreHz[(size_t) b]), hi = std::log (bandCentreHz[(size_t) b + 1]);
        binBand[(size_t) k] = b;
        binFrac[(size_t) k] = juce::jlimit (0.0f, 1.0f, (std::log (hz) - lo) / (hi - lo));
    }

    for (int i = 0; i <= CarveDisplayData::numPoints; ++i)
    {
        const float hz = CarveDisplayData::minHz * std::pow (CarveDisplayData::maxHz / CarveDisplayData::minHz,
                                                             ((float) i - 0.5f) / (float) (CarveDisplayData::numPoints - 1));
        pointEdges[(size_t) i] = juce::jlimit (1, numBins - 1, juce::roundToInt (hz / binHz));
    }

    setParams (amount, lowCut, highCut, 150.0f);
    reset();
}

void SpectralCarver::reset()
{
    std::fill (vocalFifo.begin(), vocalFifo.end(), 0.0f);
    for (auto& f : inFifo)  std::fill (f.begin(), f.end(), 0.0f);
    for (auto& f : outFifo) std::fill (f.begin(), f.end(), 0.0f);
    bandGainDb.fill (0.0f);
    std::fill (binGain.begin(), binGain.end(), 1.0f);
    pos = 0;
    hopCounter = 0;
}

void SpectralCarver::setParams (float amount01, float lowCutHz, float highCutHz, float releaseMs)
{
    amount = amount01;
    lowCut = lowCutHz;
    highCut = juce::jmax (highCutHz, lowCutHz * 1.1f);
    const float hopSeconds = (float) hopSize / (float) sr;
    attackCoeff  = std::exp (-hopSeconds / 0.012f);
    releaseCoeff = std::exp (-hopSeconds / juce::jmax (0.01f, releaseMs * 0.001f));
}

// 1 inside the Low Cut..High Cut range, fading to 0 over half an octave outside it
float SpectralCarver::rangeWeight (float hz) const
{
    float w = 1.0f;
    if (hz < lowCut)  w = 1.0f - 2.0f * std::log2 (lowCut / hz);
    if (hz > highCut) w = 1.0f - 2.0f * std::log2 (hz / highCut);
    return juce::jlimit (0.0f, 1.0f, w);
}

void SpectralCarver::process (juce::AudioBuffer<float>& beat, int numBeatChannels, const float* vocal, int numSamples)
{
    numBeatChannels = juce::jmin (numBeatChannels, (int) inFifo.size(), beat.getNumChannels());
    constexpr int mask = fftSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        vocalFifo[(size_t) pos] = vocal[i];
        for (int ch = 0; ch < numBeatChannels; ++ch)
        {
            auto* data = beat.getWritePointer (ch);
            inFifo[(size_t) ch][(size_t) pos] = data[i];
            data[i] = outFifo[(size_t) ch][(size_t) pos];
            outFifo[(size_t) ch][(size_t) pos] = 0.0f;
        }
        pos = (pos + 1) & mask;

        if (++hopCounter == hopSize)
        {
            hopCounter = 0;
            processFrame (numBeatChannels);
        }
    }
}

void SpectralCarver::processFrame (int numBeatChannels)
{
    constexpr int mask = fftSize - 1;
    auto loadFrame = [this] (const std::vector<float>& fifo)
    {
        for (int n = 0; n < fftSize; ++n)
            frame[(size_t) n] = fifo[(size_t) ((pos + n) & (fftSize - 1))] * window[(size_t) n];
        std::fill (frame.begin() + fftSize, frame.end(), 0.0f);
        fft.performRealOnlyForwardTransform (frame.data(), true);
    };
    auto power = [this] (int k) { return frame[(size_t) (2 * k)] * frame[(size_t) (2 * k)] + frame[(size_t) (2 * k + 1)] * frame[(size_t) (2 * k + 1)]; };
    const auto toDb = [this] (float p) { return 10.0f * std::log10 (p + 1.0e-20f) - refDb; };

    // 1. Where is the vocal right now?
    loadFrame (vocalFifo);
    for (int k = 0; k < numBins; ++k)
        vocalPower[(size_t) k] = power (k);

    std::array<float, numBands> vocalDb {};
    float loudest = -200.0f;
    for (int b = 0; b < numBands; ++b)
    {
        float sum = 0.0f;
        for (int k = bandEdges[(size_t) b]; k < bandEdges[(size_t) b + 1]; ++k)
            sum += vocalPower[(size_t) k];
        vocalDb[(size_t) b] = toDb (sum);
        if (rangeWeight (bandCentreHz[(size_t) b]) > 0.0f)
            loudest = juce::jmax (loudest, vocalDb[(size_t) b]);
    }

    // 2. Carve depth per band: strongest vocal bands get the most, bands 30 dB below get none.
    //    A quiet or silent vocal fades the carving out entirely.
    const float vocalPresent = juce::jlimit (0.0f, 1.0f, (loudest + 60.0f) / 20.0f);
    for (int b = 0; b < numBands; ++b)
    {
        const float strength = juce::jlimit (0.0f, 1.0f, (vocalDb[(size_t) b] - (loudest - 30.0f)) / 30.0f);
        const float target = -amount * maxDepthDb * strength * vocalPresent * rangeWeight (bandCentreHz[(size_t) b]);
        auto& g = bandGainDb[(size_t) b];
        const float c = target < g ? attackCoeff : releaseCoeff;
        g = c * g + (1.0f - c) * target;
    }
    for (int k = 0; k < numBins; ++k)
    {
        const int b = binBand[(size_t) k];
        const float db = juce::jmap (binFrac[(size_t) k], bandGainDb[(size_t) b], bandGainDb[(size_t) b + 1]);
        binGain[(size_t) k] = juce::Decibels::decibelsToGain (db);
    }

    // 3. Apply to each beat channel and overlap-add back into the output
    std::fill (beatPowerIn.begin(), beatPowerIn.end(), 0.0f);
    std::fill (beatPowerOut.begin(), beatPowerOut.end(), 0.0f);
    constexpr float olaScale = 0.5f; // 4 overlapping Hann windows sum to 2
    for (int ch = 0; ch < numBeatChannels; ++ch)
    {
        loadFrame (inFifo[(size_t) ch]);
        for (int k = 0; k < numBins; ++k)
        {
            const float p = power (k);
            const float g = binGain[(size_t) k];
            beatPowerIn[(size_t) k] += p;
            beatPowerOut[(size_t) k] += p * g * g;
            frame[(size_t) (2 * k)] *= g;
            frame[(size_t) (2 * k + 1)] *= g;
        }
        fft.performRealOnlyInverseTransform (frame.data());

        auto& out = outFifo[(size_t) ch];
        for (int n = 0; n < fftSize; ++n)
            out[(size_t) ((pos + n) & mask)] += frame[(size_t) n] * window[(size_t) n] * olaScale;
    }

    // 4. Publish spectra for the editor
    const float chNorm = 1.0f / (float) juce::jmax (1, numBeatChannels);
    for (int i = 0; i < CarveDisplayData::numPoints; ++i)
    {
        const int lo = pointEdges[(size_t) i];
        const int hi = juce::jmax (lo + 1, pointEdges[(size_t) i + 1]);
        float in = 0.0f, out = 0.0f, voc = 0.0f;
        for (int k = lo; k < hi; ++k)
        {
            in += beatPowerIn[(size_t) k];
            out += beatPowerOut[(size_t) k];
            voc += vocalPower[(size_t) k];
        }
        const float n = 1.0f / (float) (hi - lo);
        display.beatIn[(size_t) i]  = numBeatChannels > 0 ? toDb (in * n * chNorm) : -120.0f;
        display.beatOut[(size_t) i] = numBeatChannels > 0 ? toDb (out * n * chNorm) : -120.0f;
        display.vocal[(size_t) i]   = toDb (voc * n);
        display.reductionDb[(size_t) i] = juce::Decibels::gainToDecibels (binGain[(size_t) ((lo + hi) / 2)], -60.0f);
    }
}
