#include "DspEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Engine = gateverb::DspEngine;
using Curve = Engine::Curve;
constexpr double pi = 3.14159265358979323846;

void require (bool condition, const std::string& message)
{
    if (! condition) throw std::runtime_error (message);
}

Curve constantCurve (float value)
{
    Curve curve {};
    curve.fill (value);
    return curve;
}

struct Audio
{
    explicit Audio (int size) : left (static_cast<std::size_t> (size)), right (left) {}
    std::vector<float> left, right;
};

void render (Engine& engine, Audio& audio, const Curve& curve,
             double sr, int blockSize, double ppqStart = 0.17)
{
    for (int start = 0; start < static_cast<int> (audio.left.size()); start += blockSize)
    {
        const auto count = std::min (blockSize, static_cast<int> (audio.left.size()) - start);
        Engine::Transport transport;
        transport.bpm = 127.0;
        transport.ppq = ppqStart + static_cast<double> (start) * 127.0 / (60.0 * sr);
        transport.hasPpq = true;
        transport.playing = true;
        engine.process (audio.left.data() + start, audio.right.data() + start,
                        count, curve, transport);
    }
}

double energy (const Audio& audio, int start, int end)
{
    double result = 0.0;
    for (int i = start; i < end; ++i)
    {
        const auto index = static_cast<std::size_t> (i);
        result += audio.left[index] * audio.left[index]
                + audio.right[index] * audio.right[index];
    }
    return result;
}

void requireFinite (const Audio& audio, float ceiling, const std::string& name)
{
    for (const auto* channel : { &audio.left, &audio.right })
        for (float value : *channel)
            require (std::isfinite (value) && std::abs (value) <= ceiling,
                     name + ": non-finite or unbounded output");
}

void testSilenceAndDry()
{
    Engine engine;
    Engine::Parameters parameters;
    engine.prepare (48000.0, 512);
    Audio silence (48000);
    render (engine, silence, constantCurve (1.0f), 48000.0, 127);
    require (energy (silence, 0, 48000) == 0.0, "Silence must remain silent");

    parameters.mix = 0.0f;
    parameters.inputDb = -6.0f;
    parameters.outputDb = 6.0f;
    engine.setParameters (parameters);
    engine.reset();
    Audio input (48000);
    for (std::size_t i = 0; i < input.left.size(); ++i)
    {
        input.left[i] = 0.3f * std::sin (static_cast<float> (i) * 0.071f);
        input.right[i] = -0.2f * std::cos (static_cast<float> (i) * 0.023f);
    }
    auto original = input;
    render (engine, input, constantCurve (0.0f), 48000.0, 19);
    for (std::size_t i = 0; i < input.left.size(); ++i)
    {
        require (std::abs (input.left[i] - original.left[i]) < 2.0e-6f,
                 "Dry path must retain unity after cancelling IN/OUT gains");
        require (std::abs (input.right[i] - original.right[i]) < 2.0e-6f,
                 "The gate must not affect the dry right channel");
    }
}

void testReverbAndGate()
{
    Engine::Parameters p;
    p.mix = 1.0f;
    p.preDelayMs = 40.0f;
    p.decaySeconds = 1.3f;
    p.depth = 1.0f;
    Engine engine;
    engine.prepare (48000.0, 512);
    engine.setParameters (p);
    Audio impulse (6 * 48000);
    impulse.left[0] = impulse.right[0] = 0.5f;
    render (engine, impulse, constantCurve (1.0f), 48000.0, 256);
    requireFinite (impulse, 2.0f, "Impulse response");
    require (energy (impulse, 0, 1800) == 0.0, "Predelay must delay wet onset");
    const auto early = energy (impulse, 1920, 48000);
    const auto late = energy (impulse, 5 * 48000, 6 * 48000);
    require (early > 1.0e-6, "Reverb must produce a nonzero tail");
    require (late < early * 0.001, "Tail must decay substantially after several RT60 periods");

    engine.reset();
    Audio closed (48000);
    closed.left[0] = closed.right[0] = 0.5f;
    render (engine, closed, constantCurve (0.0f), 48000.0, 256);
    require (energy (closed, 0, 48000) < 1.0e-16,
             "A fully closed gate at 100% wet must be silent");

    p.gateEnabled = false;
    engine.setParameters (p);
    engine.reset();
    Audio bypassed (48000);
    bypassed.left[0] = bypassed.right[0] = 0.5f;
    render (engine, bypassed, constantCurve (0.0f), 48000.0, 512);
    require (energy (bypassed, 1920, 48000) > 1.0e-6,
             "Disabling the gate must reveal the tail even with a zero curve");
}

void testSyncAndBlockSizes()
{
    Curve curve {};
    for (std::size_t i = 0; i < curve.size(); ++i)
        curve[i] = static_cast<float> (0.5 + 0.5 * std::sin (2.0 * pi * i / curve.size()));
    Engine::Parameters p;
    p.mix = 0.8f;
    p.division = 4;
    p.preDelayMs = 11.0f;
    p.smoothMs = 0.7f;
    Audio input (96000);
    for (std::size_t i = 0; i < input.left.size(); ++i)
    {
        input.left[i] = 0.2f * std::sin (static_cast<float> (i) * 0.047f);
        input.right[i] = 0.2f * std::cos (static_cast<float> (i) * 0.063f);
    }
    Engine referenceEngine;
    referenceEngine.prepare (48000.0, 512);
    referenceEngine.setParameters (p);
    auto reference = input;
    render (referenceEngine, reference, curve, 48000.0, 512);
    for (int blockSize : { 1, 17, 127, 4096 })
    {
        Engine candidate;
        candidate.prepare (48000.0, blockSize);
        candidate.setParameters (p);
        auto result = input;
        render (candidate, result, curve, 48000.0, blockSize);
        float difference = 0.0f;
        for (std::size_t i = 0; i < result.left.size(); ++i)
            difference = std::max ({ difference, std::abs (result.left[i] - reference.left[i]),
                                    std::abs (result.right[i] - reference.right[i]) });
        require (difference < 2.0e-5f, "Realtime/offline rendering must be block-size independent");
    }

    Engine clock;
    clock.prepare (48000.0, 512);
    p.division = 6;
    p.phaseOffset = 0.125f;
    clock.setParameters (p);
    Audio scratch (480);
    Engine::Transport stopped { 120.0, -0.75, true, false };
    clock.process (scratch.left.data(), scratch.right.data(), 480, curve, stopped);
    require (std::abs (clock.getPhase() - 0.375f) < 1.0e-6f,
             "Stopped sync must hold the wrapped host position, including negative PPQ");
    p.sync = false;
    p.rateHz = 2.0f;
    p.phaseOffset = 0.0f;
    clock.setParameters (p);
    clock.reset();
    clock.process (scratch.left.data(), scratch.right.data(), 480, curve, stopped);
    require (std::abs (clock.getPhase() - static_cast<float> (479.0 * 2.0 / 48000.0)) < 1.0e-6f,
             "Free mode must run with a stopped host");
}

void testStabilityAndReset()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        Engine engine;
        engine.prepare (sr, 512);
        Engine::Parameters p;
        p.mix = 1.0f;
        p.decaySeconds = 20.0f;
        p.size = 1.0f;
        p.damping = 0.0f;
        p.diffusion = 1.0f;
        p.lowCutHz = 20.0f;
        p.highCutHz = 20000.0f;
        p.width = 2.0f;
        engine.setParameters (p);
        Audio input (static_cast<int> (sr * 8.0));
        for (std::size_t i = 0; i < input.left.size(); ++i)
        {
            input.left[i] = 0.25f * std::sin (static_cast<float> (i * 137.0 / sr * 2.0 * pi));
            input.right[i] = 0.25f * std::sin (static_cast<float> (i * 179.0 / sr * 2.0 * pi));
        }
        render (engine, input, constantCurve (1.0f), sr, 509);
        requireFinite (input, 8.0f, "Maximum decay/diffusion at " + std::to_string (sr));
        engine.reset();
        Audio silence (4096);
        render (engine, silence, constantCurve (1.0f), sr, 127);
        require (energy (silence, 0, 4096) == 0.0, "Reset must clear the recursive tail");

        p.decaySeconds = std::numeric_limits<float>::quiet_NaN();
        p.lowCutHz = std::numeric_limits<float>::infinity();
        p.mix = std::numeric_limits<float>::quiet_NaN();
        engine.setParameters (p);
        silence.left[0] = std::numeric_limits<float>::infinity();
        silence.right[0] = std::numeric_limits<float>::quiet_NaN();
        render (engine, silence, constantCurve (1.0f), sr, 256);
        requireFinite (silence, 8.0f, "Invalid host values");
    }
}
} // namespace

int main()
{
    try
    {
        testSilenceAndDry();
        testReverbAndGate();
        testSyncAndBlockSizes();
        testStabilityAndReset();
        std::cout << "PASS: silence, dry gain, predelay, reverb decay, wet gate, bypass, "
                     "sync, variable blocks, free clock, stability, reset, invalid input\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
