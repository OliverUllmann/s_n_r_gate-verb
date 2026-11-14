#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace gateverb
{
/** A stereo reverb followed by a drawable amplitude modulator.
    This class has no JUCE dependency, so the audio math can be tested on its own.
    All memory is allocated in prepare(); process() never allocates or locks.
*/
class DspEngine
{
public:
    static constexpr int curveSize = 256;
    using Curve = std::array<float, curveSize>;

    struct Parameters
    {
        float inputDb = 0.0f, outputDb = 0.0f, mix = 0.35f;
        float preDelayMs = 25.0f, decaySeconds = 3.2f, size = 0.65f;
        float damping = 0.5f, diffusion = 0.75f;
        float lowCutHz = 120.0f, highCutHz = 12000.0f, width = 1.0f;
        float depth = 1.0f, smoothMs = 5.0f, rateHz = 1.0f, phaseOffset = 0.0f;
        bool sync = true, gateEnabled = true;
        int division = 6;
    };

    struct Transport
    {
        double bpm = 120.0, ppq = 0.0;
        bool hasPpq = false, playing = false;
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const Parameters&) noexcept;
    void process (float* left, float* right, int count,
                  const Curve&, const Transport&) noexcept;

    float getPhase() const noexcept     { return displayedPhase; }
    float getGateValue() const noexcept { return gateGain; }
    float getInputPeak() const noexcept { return inputPeak; }
    float getOutputPeak() const noexcept { return outputPeak; }
    float getWetPeak() const noexcept   { return wetPeak; }

    // Musical periods are measured in quarter notes. The bar labels assume 4/4.
    static double beatsForDivision (int index) noexcept;

private:
    struct Delay
    {
        std::vector<float> buffer;
        std::size_t writeIndex = 0;
        void resize (std::size_t samples);
        void clear() noexcept;
        void write (float value) noexcept;
        float read (float delaySamples) const noexcept;
    };

    struct Smoother
    {
        float value = 0.0f, target = 0.0f, coefficient = 1.0f;
        float next() noexcept { value += coefficient * (target - value); return value; }
        void snap() noexcept { value = target; }
    };

    struct Oscillator
    {
        double sine = 0.0, cosine = 1.0, stepSine = 0.0, stepCosine = 1.0;
        double initialPhase = 0.0;
        void prepare (double frequency, double phase, double sampleRate) noexcept;
        void reset() noexcept;
        float next() noexcept;
        void normalise() noexcept;
    };

    void initialiseSmoothers() noexcept;
    void updateCoefficients() noexcept;
    static float finiteClamp (float value, float lower, float upper, float fallback) noexcept;
    static double wrap (double phase) noexcept;

    Parameters parameters;
    double sampleRate = 48000.0;
    bool prepared = false, firstProcess = true;
    std::array<Delay, 2> preDelay;
    std::array<std::array<Delay, 4>, 2> diffusers;
    std::array<std::array<float, 4>, 2> diffuserSamples {};
    std::array<Delay, 8> tank;
    std::array<float, 8> tankBaseSamples {}, tankDamping {}, feedbackGain {};
    std::array<Oscillator, 8> oscillators;
    std::array<std::array<float, 2>, 2> hpInput {}, hpOutput {}, outputLowPass {};

    Smoother inputGain, outputGain, mix, preDelaySamples, roomSize, decay;
    Smoother damping, diffusion, lowCut, highCut, stereoWidth, gateDepth, freeRate;
    float hpCoefficient = 0.98f, lpCoefficient = 0.9f, dampingCoefficient = 0.3f;
    float gateCoefficient = 0.01f, gateGain = 1.0f, displayedPhase = 0.0f;
    float widthNormalisation = 1.0f;
    float inputPeak = 0.0f, outputPeak = 0.0f, wetPeak = 0.0f;
    double freePhase = 0.0, fallbackPhase = 0.0;
    unsigned int controlCounter = 0, oscillatorCounter = 0;
};

using Parameters = DspEngine::Parameters;
using Transport = DspEngine::Transport;
} // namespace gateverb
