#include "SoundEngine.h"

const std::vector<SoundPreset>& soundPresets()
{
    //                        engine bell  bark  decay tone  tRate tDepth chorus wobble age  hiss  reverb
    static const std::vector<SoundPreset> list {
        { "Suitcase Classic",      0, 0.55f, 0.35f, 1.0f, 0.70f, 4.5f, 0.45f, 0.10f, 0.00f, 0.10f, 0.00f, 0.22f },
        { "Mellow '73 Stage",      0, 0.30f, 0.20f, 1.1f, 0.50f, 4.0f, 0.00f, 0.15f, 0.00f, 0.10f, 0.00f, 0.20f },
        { "Dyno Bright",           0, 0.95f, 0.30f, 0.9f, 0.90f, 5.0f, 0.15f, 0.40f, 0.00f, 0.05f, 0.00f, 0.25f },
        { "Barking Stage",         0, 0.50f, 0.95f, 0.9f, 0.75f, 4.5f, 0.00f, 0.05f, 0.00f, 0.45f, 0.00f, 0.18f },
        { "Lo-Fi Rhodes",          0, 0.40f, 0.30f, 1.0f, 0.45f, 3.5f, 0.20f, 0.10f, 0.45f, 0.55f, 0.45f, 0.20f },
        { "Velvet Chorus EP",      0, 0.45f, 0.25f, 1.2f, 0.60f, 3.0f, 0.10f, 0.75f, 0.05f, 0.15f, 0.00f, 0.40f },
        { "Cassette Upright",      1, 0.50f, 0.20f, 0.9f, 0.55f, 4.0f, 0.00f, 0.00f, 0.35f, 0.45f, 0.35f, 0.18f },
        { "Dusty Tape Grand",      1, 0.70f, 0.30f, 1.1f, 0.70f, 4.0f, 0.00f, 0.05f, 0.20f, 0.65f, 0.30f, 0.25f },
        { "Warped Lo-Fi Keys",     1, 0.45f, 0.20f, 0.8f, 0.45f, 4.0f, 0.00f, 0.10f, 0.85f, 0.60f, 0.50f, 0.22f },
        { "Felt Tape Piano",       1, 0.15f, 0.05f, 1.0f, 0.35f, 4.0f, 0.00f, 0.00f, 0.25f, 0.35f, 0.25f, 0.40f },
    };
    return list;
}

//==============================================================================

void KeysVoice::startNote (int note, float velocity, juce::SynthesiserSound*, int)
{
    sr = getSampleRate();
    engine = settings.engine;
    vel = juce::jlimit (0.05f, 1.0f, velocity);
    released = false;
    const double f = juce::MidiMessage::getMidiNoteInHertz (note);
    const float pos = juce::jlimit (0.0f, 1.0f, (float) (note - 21) / 87.0f); // 0 = lowest key, 1 = highest
    pan = 0.5f + 0.35f * (pos - 0.5f);                                          // low notes left, high right
    attack = 0.0f;
    attackStep = 1.0f / (float) (0.002 * sr);
    auto coef = [this] (float seconds) { return std::exp (-1.0f / (juce::jmax (0.001f, seconds) * (float) sr)); };

    if (engine == 0)
    {
        // Tine piano: carrier + 1:1 FM for the bark, plus a fast-decaying high tine "ping"
        phase = modPhase = tinePhase = 0.0;
        inc = juce::MathConstants<double>::twoPi * f / sr;
        tineInc = juce::MathConstants<double>::twoPi * juce::jmin (f * 7.02, sr * 0.45) / sr;
        bell = settings.bell;
        modIndex = (0.35f + settings.bark * 2.2f) * vel * (1.15f - 0.6f * pos);
        modCoef = coef (0.18f + 0.55f * (1.0f - pos));
        tineAmp = (0.25f + 0.75f * vel) * settings.bell * (0.6f + 0.6f * pos);
        tineCoef = coef (0.05f + 0.04f * (1.0f - pos));
        drive = 1.0f + settings.bark * 3.0f * vel;
        amp = 0.085f * std::pow (vel, 1.4f);
        ampCoef = coef (settings.decay * juce::jlimit (0.6f, 7.0f, 6.0f * std::pow (2.0f, -(float) (note - 40) / 18.0f)));
        releaseCoef = coef (0.09f);
    }
    else
    {
        // Piano: stretched (inharmonic) partials, each with a fast and a slow decay
        const float B = 0.00012f * std::pow (2.0f, (float) (note - 48) / 13.0f);
        const float tau1 = settings.decay * juce::jlimit (0.8f, 11.0f, 9.0f * std::pow (2.0f, -(float) (note - 36) / 15.0f));
        const float brightness = 0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, settings.bell * 0.6f + vel * 0.6f);
        numPartials = 0;
        float energy = 0.0f;
        for (int k = 1; k <= maxPartials - 4; ++k)
        {
            const double fk = f * k * std::sqrt (1.0 + B * k * k);
            if (fk > sr * 0.45) break;
            float a = 1.0f / std::pow ((float) k, 1.05f);
            a *= std::exp (-(float) (k - 1) * (1.0f - brightness) * 0.45f);
            a *= 0.25f + std::abs (std::sin (juce::MathConstants<float>::pi * (float) k / 7.5f)); // hammer position
            const int i = numPartials++;
            const double w = juce::MathConstants<double>::twoPi * fk / sr;
            re[(size_t) i] = 0.0f; im[(size_t) i] = 1.0f;
            rotC[(size_t) i] = (float) std::cos (w); rotS[(size_t) i] = (float) std::sin (w);
            const float tauK = tau1 / (1.0f + 0.22f * (float) (k - 1));
            slowAmp[(size_t) i] = a * 0.55f; slowCoef[(size_t) i] = coef (tauK);
            fastAmp[(size_t) i] = a * 0.45f; fastCoef[(size_t) i] = coef (tauK * 0.12f);
            energy += a * a;
        }
        // Second string, slightly detuned, for the first few partials (gives the chorus of real strings)
        for (int k = 1; k <= 4 && numPartials < maxPartials; ++k)
        {
            const int src = k - 1;
            if (src >= numPartials) break;
            const int i = numPartials++;
            const double w = std::atan2 ((double) rotS[(size_t) src], (double) rotC[(size_t) src]) * 1.0011;
            re[(size_t) i] = 0.0f; im[(size_t) i] = 1.0f;
            rotC[(size_t) i] = (float) std::cos (w); rotS[(size_t) i] = (float) std::sin (w);
            slowAmp[(size_t) i] = slowAmp[(size_t) src] * 0.6f; slowCoef[(size_t) i] = slowCoef[(size_t) src];
            fastAmp[(size_t) i] = fastAmp[(size_t) src] * 0.6f; fastCoef[(size_t) i] = fastCoef[(size_t) src];
        }
        const float norm = 1.0f / std::sqrt (juce::jmax (0.01f, energy));
        for (int i = 0; i < numPartials; ++i)
        {
            slowAmp[(size_t) i] *= norm;
            fastAmp[(size_t) i] *= norm;
        }
        amp = 0.06f * std::pow (vel, 1.3f);
        ampCoef = 1.0f;
        releaseCoef = coef (note > 88 ? 1.5f : 0.12f); // the top keys have no dampers
        hammer = 0.08f * vel * (0.4f + settings.bark);
        hammerCoef = coef (0.006f);
        renorm = 0;
    }
}

void KeysVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
        released = true;
    else
    {
        clearCurrentNote();
        amp = 0.0f;
    }
}

float KeysVoice::renderTine()
{
    const float m = (float) std::sin (modPhase) * modIndex;
    const float carrier = (float) std::sin (phase + m);
    const float ping = (float) std::sin (tinePhase) * tineAmp;
    phase += inc; modPhase += inc; tinePhase += tineInc;
    if (phase > 1.0e4) { phase = std::fmod (phase, juce::MathConstants<double>::twoPi); modPhase = std::fmod (modPhase, juce::MathConstants<double>::twoPi); }
    if (tinePhase > 1.0e4) tinePhase = std::fmod (tinePhase, juce::MathConstants<double>::twoPi);
    modIndex *= modCoef;
    tineAmp *= tineCoef;
    // Pickup: asymmetric soft saturation gives the growl when played hard
    const float x = carrier * 0.8f + ping * 0.5f;
    const float bias = 0.12f;
    return (std::tanh (drive * (x + bias)) - std::tanh (drive * bias)) / std::tanh (drive);
}

float KeysVoice::renderPiano()
{
    float s = 0.0f;
    for (int i = 0; i < numPartials; ++i)
    {
        const auto k = (size_t) i;
        const float r = re[k] * rotC[k] - im[k] * rotS[k];
        const float m = re[k] * rotS[k] + im[k] * rotC[k];
        re[k] = r; im[k] = m;
        s += m * (slowAmp[k] + fastAmp[k]);
        slowAmp[k] *= slowCoef[k];
        fastAmp[k] *= fastCoef[k];
    }
    if (++renorm >= 256)
    {
        renorm = 0;
        for (int i = 0; i < numPartials; ++i)
        {
            const auto k = (size_t) i;
            const float mag = std::sqrt (re[k] * re[k] + im[k] * im[k]);
            if (mag > 0.0f) { re[k] /= mag; im[k] /= mag; }
        }
    }
    // Hammer thump: a short burst of filtered noise
    if (hammer > 1.0e-5f)
    {
        noiseState += 0.3f * (rng.nextFloat() * 2.0f - 1.0f - noiseState);
        s += noiseState * hammer;
        hammer *= hammerCoef;
    }
    return s;
}

void KeysVoice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (! isVoiceActive())
        return;
    auto* L = out.getWritePointer (0);
    auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const float gl = std::sqrt (1.0f - pan), gr = std::sqrt (pan);

    for (int i = start; i < start + num; ++i)
    {
        const float raw = engine == 0 ? renderTine() : renderPiano();
        if (attack < 1.0f) attack = juce::jmin (1.0f, attack + attackStep);
        const float v = raw * amp * attack;
        amp *= released ? releaseCoef : ampCoef;
        L[i] += v * gl;
        if (R != nullptr) R[i] += v * gr;
        if (amp < 1.0e-5f)
        {
            clearCurrentNote();
            break;
        }
    }
}

//==============================================================================

void FxChain::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.setResonance (0.6f);
    chorus.prepare (spec);
    chorus.setRate (0.55f);
    chorus.setDepth (0.25f);
    chorus.setCentreDelay (8.0f);
    chorus.setFeedback (0.0f);
    tapeDelay.prepare (spec);
    tapeDelay.setMaximumDelayInSamples ((int) (sampleRate * 0.03));
    reverb.prepare (spec);
    gain.reset (sampleRate, 0.05);
    reset();
}

void FxChain::reset()
{
    toneFilter.reset();
    chorus.reset();
    tapeDelay.reset();
    reverb.reset();
    ageLp.fill (0.0f);
    hissLp.fill (0.0f);
}

void FxChain::process (juce::AudioBuffer<float>& buffer, const FxSettings& s)
{
    const int n = buffer.getNumSamples();
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // Tone: low-pass from about 1.2 kHz (dark) to 20 kHz (open)
    toneFilter.setCutoffFrequency (juce::jmin ((float) sr * 0.45f, 1200.0f * std::pow (16.0f, s.tone)));
    toneFilter.process (ctx);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // Tremolo: stereo auto-pan like a suitcase piano
    if (s.tremDepth > 0.001f)
    {
        const double step = juce::MathConstants<double>::twoPi * s.tremRate / sr;
        for (int i = 0; i < n; ++i)
        {
            const float lfo = (float) std::sin (tremPhase);
            tremPhase += step;
            L[i] *= 1.0f - s.tremDepth * 0.5f * (1.0f + lfo);
            R[i] *= 1.0f - s.tremDepth * 0.5f * (1.0f - lfo);
        }
        tremPhase = std::fmod (tremPhase, juce::MathConstants<double>::twoPi);
    }

    if (s.chorus > 0.001f)
    {
        chorus.setMix (s.chorus * 0.5f);
        chorus.process (ctx);
    }

    // Tape: wow and flutter (pitch wobble), saturation, high-end loss and hiss
    if (s.wobble > 0.001f || s.age > 0.001f || s.hiss > 0.001f)
    {
        const double wowStep = juce::MathConstants<double>::twoPi * 0.55 / sr;
        const double flutterStep = juce::MathConstants<double>::twoPi * 6.8 / sr;
        const float lpCoef = std::exp (-juce::MathConstants<float>::twoPi * (18000.0f - 12500.0f * s.age) / (float) sr);
        const float driveAmt = 1.0f + 3.0f * s.age;
        for (int i = 0; i < n; ++i)
        {
            const float delayMs = 6.0f + s.wobble * (3.0f * (float) std::sin (wowPhase) + 0.35f * (float) std::sin (flutterPhase));
            wowPhase += wowStep;
            flutterPhase += flutterStep;
            const float d = delayMs * 0.001f * (float) sr;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = ch == 0 ? L : R;
                tapeDelay.pushSample (ch, data[i]);
                float y = s.wobble > 0.001f ? tapeDelay.popSample (ch, d) : data[i];
                y = std::tanh (y * driveAmt) / driveAmt; // same level for quiet signals, rounds off the peaks
                ageLp[(size_t) ch] = lpCoef * ageLp[(size_t) ch] + (1.0f - lpCoef) * y;
                y = s.age > 0.001f ? ageLp[(size_t) ch] : y;
                hissLp[(size_t) ch] += 0.5f * ((rng.nextFloat() * 2.0f - 1.0f) - hissLp[(size_t) ch]);
                data[i] = y + hissLp[(size_t) ch] * s.hiss * 0.006f;
            }
        }
        wowPhase = std::fmod (wowPhase, juce::MathConstants<double>::twoPi);
        flutterPhase = std::fmod (flutterPhase, juce::MathConstants<double>::twoPi);
    }

    if (s.reverb > 0.001f)
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.78f;
        p.damping = 0.55f;
        p.width = 1.0f;
        p.wetLevel = s.reverb * 0.45f;
        p.dryLevel = 1.0f - s.reverb * 0.3f;
        reverb.setParameters (p);
        reverb.process (ctx);
    }

    gain.setTargetValue (juce::Decibels::decibelsToGain (s.volume));
    for (int i = 0; i < n; ++i)
    {
        const float g = gain.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
}
