#include "DspEngine.h"

#include <algorithm>
#include <cmath>

namespace gateverb
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr std::array<float, 8> tankMilliseconds { 29.7f, 37.1f, 41.1f, 43.7f,
                                                53.3f, 59.9f, 67.7f, 73.3f };
constexpr std::array<double, 8> modulationHz { 0.071, 0.091, 0.113, 0.137,
                                             0.163, 0.191, 0.223, 0.251 };
constexpr std::array<float, 8> injectL { 1, 1, 1, 1, -1, -1, -1, -1 };
constexpr std::array<float, 8> injectR { 1, -1, 1, -1, 1, -1, 1, -1 };
constexpr std::array<float, 8> collectL { 1, 1, -1, -1, 1, 1, -1, -1 };
constexpr std::array<float, 8> collectR { 1, -1, -1, 1, 1, -1, -1, 1 };

float clean (float x) noexcept
{
    // Hosts occasionally supply non-finite input. Never feed it into a recursive tank.
    return std::isfinite (x) ? x : 0.0f;
}

float flushTiny (float x) noexcept
{
    return std::abs (x) < 1.0e-25f ? 0.0f : x;
}

float smoothingCoefficient (double seconds, double sampleRate) noexcept
{
    return static_cast<float> (-std::expm1 (-1.0 / (seconds * sampleRate)));
}
} // namespace

void DspEngine::Delay::resize (std::size_t samples)
{
    buffer.assign (std::max<std::size_t> (8, samples), 0.0f);
    writeIndex = 0;
}

void DspEngine::Delay::clear() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

void DspEngine::Delay::write (float value) noexcept
{
    buffer[writeIndex] = value;
    if (++writeIndex == buffer.size())
        writeIndex = 0;
}

float DspEngine::Delay::read (float samples) const noexcept
{
    // The index points to the next write: one sample reads the newest stored value.
    const auto delay = std::clamp (samples, 1.0f, static_cast<float> (buffer.size() - 2));
    const auto integer = static_cast<std::size_t> (delay);
    const float fraction = delay - static_cast<float> (integer);
    const auto indexA = (writeIndex + buffer.size() - integer) % buffer.size();
    const auto indexB = indexA == 0 ? buffer.size() - 1 : indexA - 1;
    return buffer[indexA] + fraction * (buffer[indexB] - buffer[indexA]);
}

void DspEngine::Oscillator::prepare (double frequency, double phase, double sr) noexcept
{
    initialPhase = phase;
    const auto angle = 2.0 * pi * frequency / sr;
    stepSine = std::sin (angle);
    stepCosine = std::cos (angle);
    reset();
}

void DspEngine::Oscillator::reset() noexcept
{
    sine = std::sin (initialPhase);
    cosine = std::cos (initialPhase);
}

float DspEngine::Oscillator::next() noexcept
{
    const double oldSine = sine;
    sine = sine * stepCosine + cosine * stepSine;
    cosine = cosine * stepCosine - oldSine * stepSine;
    return static_cast<float> (oldSine);
}

void DspEngine::Oscillator::normalise() noexcept
{
    const double inverseLength = 1.0 / std::sqrt (sine * sine + cosine * cosine);
    sine *= inverseLength;
    cosine *= inverseLength;
}

float DspEngine::finiteClamp (float value, float lower, float upper, float fallback) noexcept
{
    return std::isfinite (value) ? std::clamp (value, lower, upper) : fallback;
}

double DspEngine::wrap (double phase) noexcept
{
    return phase - std::floor (phase);
}

double DspEngine::beatsForDivision (int index) noexcept
{
    constexpr std::array<double, 12> periods { 0.0625, 0.125, 1.0 / 6.0, 0.25,
                                              1.0 / 3.0, 0.5, 1.0, 1.5,
                                              2.0, 4.0, 8.0, 16.0 };
    return periods[static_cast<std::size_t> (std::clamp (index, 0, 11))];
}

void DspEngine::prepare (double sr, int maxBlockSize)
{
    (void) maxBlockSize; // The sample-by-sample engine has no block-sized scratch storage.
    sampleRate = std::isfinite (sr) ? std::clamp (sr, 8000.0, 384000.0) : 48000.0;

    for (auto& line : preDelay)
        line.resize (static_cast<std::size_t> (std::ceil (sampleRate * 0.251)) + 8);

    constexpr float diffuserMilliseconds[2][4] { { 4.3f, 8.7f, 12.1f, 17.3f },
                                                 { 5.1f, 9.3f, 13.7f, 19.1f } };
    for (std::size_t channel = 0; channel < diffusers.size(); ++channel)
        for (std::size_t stage = 0; stage < diffusers[channel].size(); ++stage)
        {
            diffuserSamples[channel][stage] = static_cast<float>
                (std::round (sampleRate * 0.001 * diffuserMilliseconds[channel][stage]));
            diffusers[channel][stage].resize
                (static_cast<std::size_t> (diffuserSamples[channel][stage]) + 8);
        }

    for (std::size_t i = 0; i < tank.size(); ++i)
    {
        tankBaseSamples[i] = static_cast<float> (sampleRate * tankMilliseconds[i] * 0.001);
        tank[i].resize (static_cast<std::size_t> (std::ceil (tankBaseSamples[i] * 1.6
                                                          + sampleRate * 0.001)) + 8);
        oscillators[i].prepare (modulationHz[i], static_cast<double> (i) * 0.731, sampleRate);
    }

    const float fast = smoothingCoefficient (0.015, sampleRate);
    const float slow = smoothingCoefficient (0.100, sampleRate);
    for (auto* sm : { &inputGain, &outputGain, &mix, &preDelaySamples, &diffusion,
                     &lowCut, &highCut, &stereoWidth, &gateDepth, &freeRate })
        sm->coefficient = fast;
    for (auto* sm : { &roomSize, &decay, &damping })
        sm->coefficient = slow;

    setParameters (parameters);
    prepared = true;
    reset();
}

void DspEngine::setParameters (const Parameters& supplied) noexcept
{
    parameters = supplied;
    parameters.inputDb = finiteClamp (supplied.inputDb, -24.0f, 24.0f, 0.0f);
    parameters.outputDb = finiteClamp (supplied.outputDb, -24.0f, 24.0f, 0.0f);
    parameters.mix = finiteClamp (supplied.mix, 0.0f, 1.0f, 0.35f);
    parameters.preDelayMs = finiteClamp (supplied.preDelayMs, 0.0f, 250.0f, 25.0f);
    parameters.decaySeconds = finiteClamp (supplied.decaySeconds, 0.2f, 20.0f, 3.2f);
    parameters.size = finiteClamp (supplied.size, 0.1f, 1.0f, 0.65f);
    parameters.damping = finiteClamp (supplied.damping, 0.0f, 1.0f, 0.5f);
    parameters.diffusion = finiteClamp (supplied.diffusion, 0.0f, 1.0f, 0.75f);
    parameters.lowCutHz = finiteClamp (supplied.lowCutHz, 20.0f, 2000.0f, 120.0f);
    parameters.highCutHz = finiteClamp (supplied.highCutHz, 2000.0f, 20000.0f, 12000.0f);
    parameters.width = finiteClamp (supplied.width, 0.0f, 2.0f, 1.0f);
    parameters.depth = finiteClamp (supplied.depth, 0.0f, 1.0f, 1.0f);
    parameters.smoothMs = finiteClamp (supplied.smoothMs, 0.2f, 50.0f, 5.0f);
    parameters.rateHz = finiteClamp (supplied.rateHz, 0.05f, 20.0f, 1.0f);
    parameters.phaseOffset = finiteClamp (supplied.phaseOffset, 0.0f, 1.0f, 0.0f);
    parameters.division = std::clamp (supplied.division, 0, 11);

    inputGain.target = std::pow (10.0f, parameters.inputDb * 0.05f);
    outputGain.target = std::pow (10.0f, parameters.outputDb * 0.05f);
    mix.target = parameters.mix;
    preDelaySamples.target = parameters.preDelayMs * static_cast<float> (sampleRate * 0.001);
    roomSize.target = parameters.size;
    decay.target = parameters.decaySeconds;
    damping.target = parameters.damping;
    diffusion.target = parameters.diffusion;
    lowCut.target = parameters.lowCutHz;
    highCut.target = parameters.highCutHz;
    stereoWidth.target = parameters.width;
    gateDepth.target = parameters.gateEnabled ? parameters.depth : 0.0f;
    freeRate.target = parameters.rateHz;
    gateCoefficient = smoothingCoefficient (parameters.smoothMs * 0.001, sampleRate);
}

void DspEngine::initialiseSmoothers() noexcept
{
    for (auto* sm : { &inputGain, &outputGain, &mix, &preDelaySamples, &roomSize,
                     &decay, &damping, &diffusion, &lowCut, &highCut, &stereoWidth,
                     &gateDepth, &freeRate })
        sm->snap();
    updateCoefficients();
}

void DspEngine::reset() noexcept
{
    for (auto& line : preDelay) line.clear();
    for (auto& channel : diffusers)
        for (auto& line : channel) line.clear();
    for (auto& line : tank) line.clear();
    for (auto& oscillator : oscillators) oscillator.reset();
    tankDamping.fill (0.0f);
    for (auto& channel : hpInput) channel.fill (0.0f);
    for (auto& channel : hpOutput) channel.fill (0.0f);
    for (auto& channel : outputLowPass) channel.fill (0.0f);
    freePhase = fallbackPhase = 0.0;
    controlCounter = oscillatorCounter = 0;
    gateGain = 1.0f;
    displayedPhase = 0.0f;
    inputPeak = outputPeak = wetPeak = 0.0f;
    firstProcess = true;
    if (prepared)
        initialiseSmoothers();
}

void DspEngine::updateCoefficients() noexcept
{
    const double maximumCutoff = sampleRate * 0.45;
    hpCoefficient = static_cast<float> (std::exp (-2.0 * pi
                      * std::min<double> (lowCut.value, maximumCutoff) / sampleRate));
    lpCoefficient = static_cast<float> (-std::expm1 (-2.0 * pi
                      * std::min<double> (highCut.value, maximumCutoff) / sampleRate));
    const double dampCutoff = 19000.0 * std::pow (0.025, damping.value);
    dampingCoefficient = static_cast<float> (-std::expm1 (-2.0 * pi
                            * std::min (dampCutoff, maximumCutoff) / sampleRate));
    widthNormalisation = 1.0f / std::sqrt (std::max (1.0f,
                                         0.5f * (1.0f + stereoWidth.value * stereoWidth.value)));

    for (std::size_t i = 0; i < feedbackGain.size(); ++i)
    {
        const double delaySeconds = tankBaseSamples[i] * (0.35 + 1.2 * roomSize.value) / sampleRate;
        // A round trip's gain gives -60 dB after DECAY seconds at low frequencies.
        // The orthogonal feedback matrix preserves energy; each gain is below one.
        feedbackGain[i] = static_cast<float> (std::pow (0.001, delaySeconds / decay.value));
    }
}

void DspEngine::process (float* left, float* right, int count,
                         const Curve& curve, const Transport& transport) noexcept
{
    inputPeak = outputPeak = wetPeak = 0.0f;
    if (left == nullptr || count <= 0 || ! prepared)
        return;

    if (firstProcess)
        initialiseSmoothers();

    const bool validPpq = transport.hasPpq && std::isfinite (transport.ppq);
    const double bpm = std::isfinite (transport.bpm) && transport.bpm > 0.0
                       ? std::clamp (transport.bpm, 1.0, 1000.0) : 120.0;
    const double period = beatsForDivision (parameters.division);
    const double syncStep = transport.playing ? bpm / (60.0 * sampleRate * period) : 0.0;
    const double syncStart = validPpq ? wrap (transport.ppq / period) : fallbackPhase;
    const float modulationSamples = static_cast<float> (sampleRate * 0.00012);

    for (int sample = 0; sample < count; ++sample)
    {
        const float inGain = inputGain.next();
        const float outGain = outputGain.next();
        const float wetMix = mix.next();
        const float pdSamples = preDelaySamples.next();
        const float sizeScale = 0.35f + 1.2f * roomSize.next();
        decay.next(); damping.next(); lowCut.next(); highCut.next();
        const float diffusionAmount = diffusion.next();
        const float width = stereoWidth.next();
        const float depth = gateDepth.next();
        const double freeStep = freeRate.next() / sampleRate;

        // Expensive coefficient calculations have a fixed sample cadence, independent
        // of host block boundaries. Offline and real-time renders use the same math.
        if (controlCounter == 0)
            updateCoefficients();
        controlCounter = (controlCounter + 1u) & 31u;

        const double phase = wrap ((parameters.sync ? syncStart + sample * syncStep : freePhase)
                                   + parameters.phaseOffset);
        const float tablePosition = static_cast<float> (phase * curveSize);
        const int curveIndex = std::min (curveSize - 1, static_cast<int> (tablePosition));
        const float fraction = tablePosition - static_cast<float> (curveIndex);
        const float pointA = finiteClamp (curve[static_cast<std::size_t> (curveIndex)], 0.0f, 1.0f, 1.0f);
        const float pointB = finiteClamp (curve[static_cast<std::size_t> ((curveIndex + 1) % curveSize)],
                                         0.0f, 1.0f, 1.0f);
        const float gateTarget = 1.0f - depth * (1.0f - (pointA + fraction * (pointB - pointA)));
        if (firstProcess)
        {
            gateGain = gateTarget;
            firstProcess = false;
        }
        else
            gateGain += gateCoefficient * (gateTarget - gateGain);
        gateGain = flushTiny (gateGain);
        displayedPhase = static_cast<float> (phase);
        freePhase = wrap (freePhase + freeStep);

        const float dryL = clean (left[sample]) * inGain;
        const float dryR = clean (right != nullptr ? right[sample] : left[sample]) * inGain;
        inputPeak = std::max ({ inputPeak, std::abs (dryL), std::abs (dryR) });
        std::array<float, 2> diffused { dryL, dryR };

        for (std::size_t channel = 0; channel < 2; ++channel)
        {
            // Two cascaded high-pass stages keep sub-bass out of the feedback tank.
            for (std::size_t stage = 0; stage < 2; ++stage)
            {
                const float filtered = hpCoefficient * (hpOutput[channel][stage]
                                      + diffused[channel] - hpInput[channel][stage]);
                hpInput[channel][stage] = diffused[channel];
                hpOutput[channel][stage] = flushTiny (filtered);
                diffused[channel] = hpOutput[channel][stage];
            }
            preDelay[channel].write (diffused[channel]);
            diffused[channel] = preDelay[channel].read (pdSamples + 1.0f);

            const float coefficient = 0.72f * diffusionAmount;
            for (std::size_t stage = 0; stage < 4; ++stage)
            {
                auto& allpass = diffusers[channel][stage];
                const float delayed = allpass.read (diffuserSamples[channel][stage]);
                const float result = delayed - coefficient * diffused[channel];
                allpass.write (flushTiny (diffused[channel] + coefficient * result));
                diffused[channel] = result;
            }
        }

        std::array<float, 8> delayed {};
        float sum = 0.0f, wetL = 0.0f, wetR = 0.0f;
        for (std::size_t i = 0; i < tank.size(); ++i)
        {
            const float delaySamples = tankBaseSamples[i] * sizeScale
                                     + oscillators[i].next() * modulationSamples * diffusionAmount;
            const float value = tank[i].read (delaySamples);
            tankDamping[i] = flushTiny (tankDamping[i] + dampingCoefficient * (value - tankDamping[i]));
            delayed[i] = tankDamping[i] * feedbackGain[i];
            sum += delayed[i];
            wetL += collectL[i] * value;
            wetR += collectR[i] * value;
        }

        for (std::size_t i = 0; i < tank.size(); ++i)
        {
            // H = I - (2/N)11^T is a Householder reflection: mixing adds no energy.
            const float reflected = delayed[i] - 0.25f * sum;
            const float injection = 0.3f * (injectL[i] * diffused[0] + injectR[i] * diffused[1]);
            tank[i].write (flushTiny (injection + reflected));
        }

        // Compensate the tank's diffusion/filter losses so the wet control has a
        // useful musical balance against the unattenuated dry path.
        std::array<float, 2> wet { wetL * 0.5f, wetR * 0.5f };
        for (std::size_t channel = 0; channel < 2; ++channel)
            for (std::size_t stage = 0; stage < 2; ++stage)
            {
                auto& state = outputLowPass[channel][stage];
                state = flushTiny (state + lpCoefficient * (wet[channel] - state));
                wet[channel] = state;
            }

        const float mid = 0.5f * (wet[0] + wet[1]);
        const float side = 0.5f * (wet[0] - wet[1]) * width;
        wetL = (mid + side) * widthNormalisation * gateGain;
        wetR = (mid - side) * widthNormalisation * gateGain;
        wetPeak = std::max ({ wetPeak, std::abs (wetL), std::abs (wetR) });

        const float outL = clean (((1.0f - wetMix) * dryL + wetMix * wetL) * outGain);
        const float outR = clean (((1.0f - wetMix) * dryR + wetMix * wetR) * outGain);
        left[sample] = outL;
        if (right != nullptr) right[sample] = outR;
        outputPeak = std::max ({ outputPeak, std::abs (outL), std::abs (outR) });

        if (++oscillatorCounter == 4096)
        {
            for (auto& oscillator : oscillators) oscillator.normalise();
            oscillatorCounter = 0;
        }
    }

    if (parameters.sync)
        fallbackPhase = wrap (syncStart + count * syncStep);
}
} // namespace gateverb
