#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
constexpr std::array<const char*, 18> parameterIds {
    "input", "output", "mix", "predelay", "decay", "size", "damping", "diffusion",
    "lowcut", "highcut", "width", "depth", "smooth", "rate", "phase", "sync", "gate", "division"
};
constexpr const char* stateType = "GateVerbState";
constexpr int stateVersion = 1;
static_assert (std::atomic<float>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free,
               "The audio curve requires lock-free native atomics.");

bool isValidGridSize (int size)
{
    return size == 4 || size == 8 || size == 16 || size == 32 || size == 64;
}

void accumulatePeak (std::atomic<float>& meter, float peak) noexcept
{
    // The UI consumes this maximum at 30 Hz, so short transients are retained.
    // Bound the retry count: even a simultaneous UI read cannot stall audio.
    float previous = meter.load (std::memory_order_relaxed);
    for (int attempt = 0; attempt < 2 && peak > previous; ++attempt)
        if (meter.compare_exchange_strong (previous, peak, std::memory_order_relaxed))
            break;
}

juce::NormalisableRange<float> range (float lo, float hi, float step, float centre = 0.0f)
{
    juce::NormalisableRange<float> result (lo, hi, step);
    if (centre > lo && centre < hi)
        result.setSkewForCentre (centre);
    return result;
}

struct FactoryPreset
{
    const char* name;
    int shape, division;
    float mix, predelay, decay, size, damping, diffusion, lowcut, highcut, width, depth, smooth;
    bool sync;
    float rate;
};

constexpr FactoryPreset factoryPresets[] {
    { "01 - Gated Bloom",       1, 6, 38,  24, 3.2f, 68, 45, 82, 150, 12500, 110, 100,  4, true,  1 },
    { "02 - Afterglow Duck",    2, 6, 40,  18, 5.4f, 82, 55, 92, 180, 10000, 135, 100,  8, true,  1 },
    { "03 - Reverse Tide",      3, 9, 48,  35, 6.8f, 90, 52, 88, 160, 11000, 140, 100,  7, true,  1 },
    { "04 - Neon Sixteenths",   1, 3, 34,   6, 2.7f, 58, 35, 80, 250, 14000, 115, 100,  2, true,  1 },
    { "05 - Broken Glass",     6, 9, 42,   8, 3.8f, 62, 27, 65, 300, 16000, 145, 100,  1, true,  1 },
    { "06 - Velvet Room",      0, 6, 22,  12, 1.1f, 32, 65, 85, 120,  8500, 100,   0,  6, true,  1 },
    { "07 - Slow Orbit",       5, 9, 48,  45, 9.0f, 94, 62, 95, 220,  9000, 150,  80, 15, false, 0.12f },
    { "08 - Dotted Halo",      2, 7, 40,  50, 4.5f, 76, 42, 88, 190, 12500, 130,  90,  5, true,  1 },
    { "09 - Drum Chamber",     4, 5, 28,   2, 0.8f, 25, 48, 70, 200, 10000,  95,  80,  2, true,  1 },
    { "10 - Cloud Machine",    0, 9, 55,  65, 14.0f,98, 60, 98, 250,  8000, 160,   0, 12, true,  1 },
    { "11 - Wide Open Send",   0, 6,100,  22, 3.8f, 78, 48, 90, 200, 11500, 130,   0,  5, true,  1 },
    { "12 - Pumping Send",     2, 6,100,  10, 5.0f, 80, 50, 90, 240, 11000, 140, 100,  6, true,  1 }
};
}

juce::AudioProcessorValueTreeState::ParameterLayout GateVerbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto add = [&layout] (const char* id, const char* name,
                               juce::NormalisableRange<float> r, float def, const char* unit)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (id, 1), name, r, def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };
    add ("input", "Input", range (-24, 24, 0.1f), 0, "dB");
    add ("output", "Output", range (-24, 12, 0.1f), 0, "dB");
    add ("mix", "Wet / Dry", range (0, 100, 0.1f), 38, "%");
    add ("predelay", "Pre-delay", range (0, 250, 0.1f, 40), 24, "ms");
    add ("decay", "Decay", range (0.2f, 20, 0.01f, 3), 3.2f, "s");
    add ("size", "Size", range (10, 100, 0.1f), 68, "%");
    add ("damping", "Damping", range (0, 100, 0.1f), 45, "%");
    add ("diffusion", "Diffusion", range (0, 100, 0.1f), 82, "%");
    add ("lowcut", "Low Cut", range (20, 2000, 1, 180), 150, "Hz");
    add ("highcut", "High Cut", range (2000, 20000, 1, 9000), 12500, "Hz");
    add ("width", "Stereo Width", range (0, 200, 0.1f), 110, "%");
    add ("depth", "Gate Depth", range (0, 100, 0.1f), 100, "%");
    add ("smooth", "Gate Smooth", range (0.2f, 50, 0.1f, 5), 4, "ms");
    add ("rate", "Free Rate", range (0.05f, 20, 0.01f, 1), 1, "Hz");
    add ("phase", "Gate Phase", range (0, 100, 0.1f), 0, "%");
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("sync", 1), "Tempo Sync", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("gate", 1), "Gate Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("division", 1), "Gate Division",
        juce::StringArray { "1/64", "1/32", "1/16 T", "1/16", "1/8 T", "1/8",
                            "1/4", "1/4 D", "1/2", "1 bar", "2 bars", "4 bars" }, 6));
    return layout;
}

GateVerbAudioProcessor::GateVerbAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, stateType, createParameterLayout())
{
    const auto initial = makeShape (1);
    for (size_t i = 0; i < curveSize; ++i)
        curveStorage[i].store (initial[i], std::memory_order_relaxed);
    audioCurve = initial;
    for (size_t i = 0; i < parameterIds.size(); ++i)
    {
        parameterValues[i] = apvts.getRawParameterValue (parameterIds[i]);
        apvts.addParameterListener (parameterIds[i], this);
    }
}

GateVerbAudioProcessor::~GateVerbAudioProcessor()
{
    for (const auto* id : parameterIds)
        apvts.removeParameterListener (id, this);
}

void GateVerbAudioProcessor::parameterChanged (const juce::String&, float)
{
    // This callback can run on the audio thread: no GUI calls, locks or allocation.
    presetDirty.store (true, std::memory_order_relaxed);
}

void GateVerbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    processingSampleRate = sampleRate > 0 ? sampleRate : 44100.0;
    scratchSize = juce::jlimit (32, 65536, samplesPerBlock);
    monoScratch.setSize (2, scratchSize);
    engine.prepare (processingSampleRate, scratchSize);
    engine.setParameters (readParameters());
    engine.reset();
    audioCurveRevision = UINT32_MAX;
    readCurveForAudio();
    prepared = true;
}

void GateVerbAudioProcessor::releaseResources()
{
    prepared = false;
    engine.reset();
    meterIn.store (0); meterOut.store (0); meterWet.store (0);
}

void GateVerbAudioProcessor::reset()
{
    engine.reset();
    meterIn.store (0); meterOut.store (0); meterWet.store (0);
    visualPhase.store (0);
}

bool GateVerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
                            || output == juce::AudioChannelSet::stereo());
}

gateverb::DspEngine::Parameters GateVerbAudioProcessor::readParameters() const noexcept
{
    std::array<float, parameterIds.size()> v {};
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = parameterValues[i]->load (std::memory_order_relaxed);
    gateverb::DspEngine::Parameters p;
    p.inputDb = v[0]; p.outputDb = v[1]; p.mix = v[2] * 0.01f;
    p.preDelayMs = v[3]; p.decaySeconds = v[4]; p.size = v[5] * 0.01f;
    p.damping = v[6] * 0.01f; p.diffusion = v[7] * 0.01f;
    p.lowCutHz = v[8]; p.highCutHz = v[9]; p.width = v[10] * 0.01f;
    p.depth = v[11] * 0.01f; p.smoothMs = v[12]; p.rateHz = v[13];
    p.phaseOffset = v[14] * 0.01f; p.sync = v[15] >= 0.5f;
    p.gateEnabled = v[16] >= 0.5f; p.division = static_cast<int> (v[17]);
    return p;
}

void GateVerbAudioProcessor::readCurveForAudio() noexcept
{
    const auto before = curveRevision.load (std::memory_order_acquire);
    if (before == audioCurveRevision || (before & 1u) != 0)
        return;

    Curve candidate;
    for (size_t i = 0; i < curveSize; ++i)
        candidate[i] = curveStorage[i].load (std::memory_order_relaxed);
    std::atomic_thread_fence (std::memory_order_acquire);
    const auto after = curveRevision.load (std::memory_order_acquire);
    if (before == after)
    {
        audioCurve = candidate;
        audioCurveRevision = after;
    }
    // A simultaneous mouse edit is simply picked up next block. Never spin/wait.
}

void GateVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const auto channels = getTotalNumInputChannels();
    const auto samples = buffer.getNumSamples();
    for (int c = channels; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, samples);
    if (! prepared || channels < 1 || samples == 0)
        return;

    engine.setParameters (readParameters());
    readCurveForAudio();
    gateverb::DspEngine::Transport transport;
    transport.playing = true; // A host without transport information gets an internal clock.
    bool hostKnown = false;
    if (auto* playhead = getPlayHead())
        if (const auto pos = playhead->getPosition())
        {
            hostKnown = true;
            transport.playing = pos->getIsPlaying();
            if (const auto bpm = pos->getBpm())
                if (std::isfinite (*bpm) && *bpm > 0)
                    transport.bpm = juce::jlimit (1.0, 999.0, *bpm);
            if (const auto ppq = pos->getPpqPosition())
                if (std::isfinite (*ppq))
                {
                    transport.ppq = *ppq;
                    transport.hasPpq = true;
                }
        }

    float inPeak = 0, outPeak = 0, wetPeak = 0;
    for (int offset = 0; offset < samples; offset += scratchSize)
    {
        const int count = std::min (scratchSize, samples - offset);
        auto chunkTransport = transport;
        if (transport.hasPpq && transport.playing)
            chunkTransport.ppq += offset * transport.bpm / (60.0 * processingSampleRate);
        if (channels == 1)
        {
            monoScratch.copyFrom (0, 0, buffer, 0, offset, count);
            monoScratch.copyFrom (1, 0, buffer, 0, offset, count);
            engine.process (monoScratch.getWritePointer (0), monoScratch.getWritePointer (1),
                            count, audioCurve, chunkTransport);
            auto* output = buffer.getWritePointer (0, offset);
            const auto* left = monoScratch.getReadPointer (0);
            const auto* right = monoScratch.getReadPointer (1);
            for (int i = 0; i < count; ++i)
                output[i] = 0.5f * (left[i] + right[i]);
        }
        else
            engine.process (buffer.getWritePointer (0, offset), buffer.getWritePointer (1, offset),
                            count, audioCurve, chunkTransport);
        inPeak = std::max (inPeak, engine.getInputPeak());
        outPeak = std::max (outPeak, engine.getOutputPeak());
        wetPeak = std::max (wetPeak, engine.getWetPeak());
    }
    accumulatePeak (meterIn, inPeak);
    accumulatePeak (meterOut, outPeak);
    accumulatePeak (meterWet, wetPeak);
    visualPhase.store (engine.getPhase(), std::memory_order_relaxed);
    visualGate.store (engine.getGateValue(), std::memory_order_relaxed);
    visualBpm.store (transport.bpm, std::memory_order_relaxed);
    visualPlaying.store (transport.playing, std::memory_order_relaxed);
    visualHasHost.store (hostKnown && transport.hasPpq, std::memory_order_relaxed);
}

double GateVerbAudioProcessor::getTailLengthSeconds() const
{
    return parameterValues[4]->load() * 2.0 + parameterValues[3]->load() * 0.001 + 0.5;
}

juce::AudioProcessorEditor* GateVerbAudioProcessor::createEditor()
{
    return new GateVerbAudioProcessorEditor (*this);
}

GateVerbAudioProcessor::Curve GateVerbAudioProcessor::getCurve() const
{
    const juce::ScopedLock lock (stateLock);
    Curve result;
    for (size_t i = 0; i < curveSize; ++i)
        result[i] = curveStorage[i].load (std::memory_order_relaxed);
    return result;
}

void GateVerbAudioProcessor::markChanged()
{
    presetDirty.store (true, std::memory_order_relaxed);
    sendChangeMessage();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void GateVerbAudioProcessor::publishCurve (const Curve& curve)
{
    const juce::ScopedLock lock (stateLock);
    curveRevision.fetch_add (1, std::memory_order_acq_rel);
    // Paired with the reader's acquire fence, including on weakly ordered CPUs.
    std::atomic_thread_fence (std::memory_order_release);
    for (size_t i = 0; i < curveSize; ++i)
        curveStorage[i].store (std::isfinite (curve[i]) ? juce::jlimit (0.0f, 1.0f, curve[i]) : 0.0f,
                               std::memory_order_relaxed);
    curveRevision.fetch_add (1, std::memory_order_release);
}

void GateVerbAudioProcessor::setCurve (const Curve& curve)
{
    publishCurve (curve);
    markChanged();
}

GateVerbAudioProcessor::Curve GateVerbAudioProcessor::makeShape (int shape)
{
    Curve c;
    constexpr float pi = 3.14159265358979323846f;
    constexpr std::array<float, 16> steps { 1, 1, 0, 0, 0.72f, 0, 0.35f, 0, 1, 0, 0.5f, 0.5f, 0, 0.8f, 0, 0 };
    for (size_t i = 0; i < curveSize; ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (curveSize);
        switch (shape)
        {
            case 1: c[i] = t < 0.5f ? 1.0f : 0.0f; break;
            case 2: c[i] = t < 0.06f ? 0.0f : std::pow (juce::jlimit (0.0f, 1.0f, (t - 0.06f) / 0.78f), 0.65f); break;
            case 3: c[i] = t * t; break;
            case 4: c[i] = (1.0f - t) * (1.0f - t); break;
            case 5: c[i] = 0.5f - 0.5f * std::cos (2.0f * pi * t); break;
            case 6: c[i] = steps[std::min (size_t (15), i * 16 / curveSize)]; break;
            default: c[i] = 1.0f; break;
        }
    }
    return c;
}

void GateVerbAudioProcessor::applyShape (int index) { setCurve (makeShape (index)); }

void GateVerbAudioProcessor::setEditorMode (int mode)
{
    mode = juce::jlimit (0, 1, mode);
    if (editorMode.exchange (mode, std::memory_order_relaxed) != mode)
    {
        sendChangeMessage();
        updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    }
}

void GateVerbAudioProcessor::setGridSize (int size)
{
    if (isValidGridSize (size) && gridSize.exchange (size, std::memory_order_relaxed) != size)
    {
        sendChangeMessage();
        updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    }
}

juce::StringArray GateVerbAudioProcessor::getFactoryPresetNames() const
{
    juce::StringArray names;
    for (const auto& preset : factoryPresets)
        names.add (preset.name);
    return names;
}

juce::String GateVerbAudioProcessor::getPresetName() const
{
    const juce::ScopedLock lock (stateLock);
    return presetName;
}

void GateVerbAudioProcessor::setParameter (const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }
}

void GateVerbAudioProcessor::loadFactoryPreset (int index)
{
    if (index < 0 || index >= static_cast<int> (std::size (factoryPresets)))
        return;
    const auto& p = factoryPresets[index];
    setParameter ("input", 0); setParameter ("output", 0); setParameter ("mix", p.mix);
    setParameter ("predelay", p.predelay); setParameter ("decay", p.decay); setParameter ("size", p.size);
    setParameter ("damping", p.damping); setParameter ("diffusion", p.diffusion);
    setParameter ("lowcut", p.lowcut); setParameter ("highcut", p.highcut); setParameter ("width", p.width);
    setParameter ("depth", p.depth); setParameter ("smooth", p.smooth); setParameter ("rate", p.rate);
    setParameter ("phase", 0); setParameter ("sync", p.sync ? 1.0f : 0.0f);
    setParameter ("gate", 1); setParameter ("division", static_cast<float> (p.division));
    setCurve (makeShape (p.shape));
    {
        const juce::ScopedLock lock (stateLock);
        presetName = p.name;
    }
    editorMode.store (p.shape == 6 || p.shape == 1 ? 1 : 0);
    presetDirty.store (false, std::memory_order_relaxed);
    sendChangeMessage();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

juce::ValueTree GateVerbAudioProcessor::makeState()
{
    const juce::ScopedLock lock (stateLock);
    auto state = apvts.copyState();
    state.setProperty ("schemaVersion", stateVersion, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("editorMode", getEditorMode(), nullptr);
    state.setProperty ("gridSize", getGridSize(), nullptr);
    state.setProperty ("presetDirty", isPresetDirty(), nullptr);
    if (auto old = state.getChildWithName ("Curve"); old.isValid())
        state.removeChild (old, nullptr);
    juce::StringArray values;
    for (const auto value : getCurve())
        values.add (juce::String (value, 7));
    juce::ValueTree curve ("Curve");
    curve.setProperty ("values", values.joinIntoString (" "), nullptr);
    state.appendChild (curve, nullptr);
    return state;
}

juce::Result GateVerbAudioProcessor::restoreState (const juce::ValueTree& source)
{
    if (! source.isValid() || source.getType() != juce::Identifier (stateType))
        return juce::Result::fail ("This file is not a GATE-VERB preset.");
    if (static_cast<int> (source.getProperty ("schemaVersion", 0)) != stateVersion)
        return juce::Result::fail ("This preset uses an unsupported file version.");
    juce::StringArray values;
    values.addTokens (source.getChildWithName ("Curve").getProperty ("values").toString(), " \t\r\n", "");
    values.removeEmptyStrings();
    if (values.size() != static_cast<int> (curveSize))
        return juce::Result::fail ("The gate curve is incomplete.");
    Curve curve;
    for (size_t i = 0; i < curveSize; ++i)
    {
        const auto text = values[static_cast<int> (i)].toStdString();
        char* end = nullptr;
        const float value = std::strtof (text.c_str(), &end);
        if (end == text.c_str() || *end != '\0' || ! std::isfinite (value) || value < 0.0f || value > 1.0f)
            return juce::Result::fail ("The preset contains an invalid gate value.");
        curve[i] = value;
    }
    // Validate every parameter before changing any state. Only validated values
    // enter the APVTS; duplicate IDs must not override a previously checked value.
    juce::ValueTree cleanState (stateType);
    for (const auto* id : parameterIds)
    {
        const auto node = source.getChildWithProperty ("id", id);
        int occurrences = 0;
        for (const auto child : source)
            if (child.getProperty ("id").toString() == id)
                ++occurrences;
        if (occurrences != 1 || ! node.hasType ("PARAM") || ! node.hasProperty ("value"))
            return juce::Result::fail ("The preset is missing parameter: " + juce::String (id));
        const auto text = node.getProperty ("value").toString().toStdString();
        char* end = nullptr;
        const auto value = std::strtof (text.c_str(), &end);
        const auto& r = apvts.getParameter (id)->getNormalisableRange();
        if (end == text.c_str() || *end != '\0' || ! std::isfinite (value) || value < r.start || value > r.end)
            return juce::Result::fail ("The preset contains an invalid parameter: " + juce::String (id));
        juce::ValueTree checked ("PARAM");
        checked.setProperty ("id", id, nullptr);
        checked.setProperty ("value", value, nullptr);
        cleanState.appendChild (checked, nullptr);
    }
    {
        const juce::ScopedLock lock (stateLock);
        apvts.replaceState (cleanState);
        publishCurve (curve);
        presetName = source.getProperty ("presetName", "User Preset").toString().substring (0, 100);
        editorMode.store (juce::jlimit (0, 1, static_cast<int> (source.getProperty ("editorMode", 0))));
        const auto restoredGrid = static_cast<int> (source.getProperty ("gridSize", 16));
        gridSize.store (isValidGridSize (restoredGrid) ? restoredGrid : 16);
        presetDirty.store (static_cast<bool> (source.getProperty ("presetDirty", false)));
    }
    sendChangeMessage();
    return juce::Result::ok();
}

void GateVerbAudioProcessor::getStateInformation (juce::MemoryBlock& block)
{
    if (auto xml = makeState().createXml())
        copyXmlToBinary (*xml, block);
}

void GateVerbAudioProcessor::setStateInformation (const void* data, int bytes)
{
    if (data == nullptr || bytes <= 0 || bytes > 4 * 1024 * 1024)
        return;
    if (auto xml = getXmlFromBinary (data, bytes))
        restoreState (juce::ValueTree::fromXml (*xml));
}

juce::File GateVerbAudioProcessor::getUserPresetDirectory()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("studiobro").getChildFile ("GATE-VERB").getChildFile ("Presets");
}

juce::Result GateVerbAudioProcessor::saveUserPreset (const juce::File& file)
{
    const auto dirResult = file.getParentDirectory().createDirectory();
    if (dirResult.failed())
        return dirResult;
    auto state = makeState();
    const auto name = file.getFileNameWithoutExtension().substring (0, 100);
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("presetDirty", false, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr)
        return juce::Result::fail ("The preset could not be encoded.");
    juce::TemporaryFile temp (file);
    if (! temp.getFile().replaceWithText (xml->toString()) || ! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("The preset could not be saved at this location.");
    {
        const juce::ScopedLock lock (stateLock);
        presetName = name;
    }
    presetDirty.store (false);
    sendChangeMessage();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

juce::Result GateVerbAudioProcessor::loadUserPreset (const juce::File& file)
{
    if (! file.existsAsFile() || file.getSize() > 4 * 1024 * 1024)
        return juce::Result::fail ("The preset file is missing or too large.");
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return juce::Result::fail ("The preset could not be read.");
    const auto result = restoreState (juce::ValueTree::fromXml (*xml));
    if (result.wasOk())
    {
        presetDirty.store (false);
        updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    }
    return result;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GateVerbAudioProcessor();
}
