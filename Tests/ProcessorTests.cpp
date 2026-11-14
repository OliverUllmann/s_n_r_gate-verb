#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
constexpr std::array<const char*, 18> ids {
    "input", "output", "mix", "predelay", "decay", "size", "damping", "diffusion",
    "lowcut", "highcut", "width", "depth", "smooth", "rate", "phase", "sync", "gate", "division"
};

void require (bool condition, const char* message)
{
    if (! condition) throw std::runtime_error (message);
}

void setActual (GateVerbAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.apvts.getParameter (id);
    require (parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

struct HostChangeProbe final : juce::AudioProcessorListener
{
    explicit HostChangeProbe (juce::AudioProcessor& p) : processor (p)
    {
        processor.addListener (this);
    }
    ~HostChangeProbe() override { processor.removeListener (this); }

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
    {
        if (details.nonParameterStateChanged)
            nonParameterChanges.fetch_add (1, std::memory_order_relaxed);
    }

    juce::AudioProcessor& processor;
    std::atomic<int> nonParameterChanges { 0 };
};

void testHostChangeNotifications()
{
    GateVerbAudioProcessor processor;
    juce::MemoryBlock cleanState;
    processor.getStateInformation (cleanState);
    const auto cleanCurve = processor.getCurve();
    const auto cleanMode = processor.getEditorMode();
    const auto cleanGrid = processor.getGridSize();
    HostChangeProbe host (processor);

    processor.setCurve (GateVerbAudioProcessor::makeShape (5));
    require (host.nonParameterChanges.exchange (0) > 0,
             "Drawing a gate must tell the host that the session needs saving");
    processor.setGridSize (cleanGrid == 32 ? 16 : 32);
    require (host.nonParameterChanges.exchange (0) > 0,
             "Changing the saved grid must tell the host that the session needs saving");
    processor.setEditorMode (cleanMode == 0 ? 1 : 0);
    require (host.nonParameterChanges.exchange (0) > 0,
             "Changing the saved draw mode must tell the host that the session needs saving");

    // Hosts restore state during plugin scanning, project load and offline
    // rendering. Recall must not mark a just-loaded project as modified.
    processor.setStateInformation (cleanState.getData(), static_cast<int> (cleanState.getSize()));
    require (host.nonParameterChanges.load() == 0,
             "Host state recall must not send a non-parameter edit notification");
    require (processor.getEditorMode() == cleanMode && processor.getGridSize() == cleanGrid,
             "Silent host recall must still restore the saved editor settings");
    require (! processor.isPresetDirty(), "Clean host state must restore the saved clean status");
    const auto recalledCurve = processor.getCurve();
    for (std::size_t i = 0; i < cleanCurve.size(); ++i)
        require (std::abs (recalledCurve[i] - cleanCurve[i]) < 1.0e-6f,
                 "Silent host recall must still restore the saved gate curve");
}

void testStateAndPresets()
{
    GateVerbAudioProcessor original;
    require (original.getFactoryPresetNames().size() >= 8, "Factory presets are missing");
    original.loadFactoryPreset (3);
    for (std::size_t i = 0; i < ids.size(); ++i)
        original.apvts.getParameter (ids[i])->setValueNotifyingHost
            (0.15f + static_cast<float> (i % 6) * 0.13f);
    GateVerbAudioProcessor::Curve curve {};
    for (std::size_t i = 0; i < curve.size(); ++i)
        curve[i] = 0.1f + 0.8f * static_cast<float> (i) / 255.0f;
    original.setCurve (curve);
    original.setEditorMode (1);
    original.setGridSize (32);

    juce::MemoryBlock state;
    original.getStateInformation (state);
    require (state.getSize() > 1000, "Host state must include the gate curve");
    GateVerbAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    for (const auto* id : ids)
        require (std::abs (original.apvts.getRawParameterValue (id)->load()
                        - restored.apvts.getRawParameterValue (id)->load()) < 0.002f,
                 "A parameter changed after a host-state roundtrip");
    const auto restoredCurve = restored.getCurve();
    for (std::size_t i = 0; i < curve.size(); ++i)
        require (std::abs (curve[i] - restoredCurve[i]) < 1.0e-6f,
                 "The drawn gate changed after a host-state roundtrip");
    require (restored.getEditorMode() == 1 && restored.getGridSize() == 32,
             "Editor draw mode/grid must be restored with the session");

    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("gateverb-tests-" + juce::Uuid().toString());
    require (directory.createDirectory().wasOk(), "Unable to create temporary preset directory");
    const auto presetFile = directory.getChildFile ("Roundtrip.gateverb");
    require (original.saveUserPreset (presetFile).wasOk(), "User preset save failed");
    restored.loadFactoryPreset (0);
    require (restored.loadUserPreset (presetFile).wasOk(), "User preset load failed");
    require (restored.getPresetName() == "Roundtrip", "Preset name was not restored");
    require (! restored.isPresetDirty(), "A loaded user preset must not be marked modified");
    for (const auto* id : ids)
        require (std::abs (original.apvts.getRawParameterValue (id)->load()
                        - restored.apvts.getRawParameterValue (id)->load()) < 0.002f,
                 "A parameter changed after a user-preset roundtrip");

    const auto validXml = juce::XmlDocument::parse (presetFile);
    require (validXml != nullptr, "Unable to read the valid preset fixture");
    std::array<float, ids.size()> previousParameters {};
    for (std::size_t i = 0; i < ids.size(); ++i)
        previousParameters[i] = restored.apvts.getRawParameterValue (ids[i])->load();
    const auto previousCurve = restored.getCurve();
    const auto previousName = restored.getPresetName();
    HostChangeProbe host (restored);
    const auto expectRejected = [&] (const juce::ValueTree& malformed, const char* failure)
    {
        const auto malformedXml = malformed.createXml();
        require (malformedXml != nullptr && presetFile.replaceWithText (malformedXml->toString()),
                 "Unable to write malformed test preset");
        require (restored.loadUserPreset (presetFile).failed(), failure);
        for (std::size_t i = 0; i < ids.size(); ++i)
            require (restored.apvts.getRawParameterValue (ids[i])->load() == previousParameters[i],
                     "Rejected preset must not partially change any parameter");
        require (restored.getCurve() == previousCurve && restored.getPresetName() == previousName
                 && ! restored.isPresetDirty(),
                 "Rejected preset must preserve the curve, name and clean status");
        require (host.nonParameterChanges.load() == 0,
                 "Rejected preset must not notify the host of a state edit");
    };
    for (const auto* invalidValue : { "25oops", "nan", "inf", "", "1e1000", "101", "-1" })
    {
        auto malformed = juce::ValueTree::fromXml (*validXml);
        // A different valid parameter precedes the malformed one: rejecting the
        // whole document must leave this earlier parameter unchanged as well.
        malformed.getChildWithProperty ("id", "input").setProperty ("value", -22.0f, nullptr);
        malformed.getChildWithProperty ("id", "mix").setProperty ("value", invalidValue, nullptr);
        expectRejected (malformed,
                        "Malformed, non-finite or out-of-range parameter text must be rejected");
    }

    auto duplicated = juce::ValueTree::fromXml (*validXml);
    auto laterMix = duplicated.getChildWithProperty ("id", "mix").createCopy();
    laterMix.setProperty ("value", 99.0f, nullptr);
    duplicated.appendChild (laterMix, nullptr);
    expectRejected (duplicated,
                    "A later duplicate PARAM with id=mix must not override the validated value");

    auto wrongType = juce::ValueTree::fromXml (*validXml);
    const auto mixNode = wrongType.getChildWithProperty ("id", "mix");
    const auto mixIndex = wrongType.indexOf (mixNode);
    juce::ValueTree disguisedMix ("WrongNodeType");
    disguisedMix.copyPropertiesFrom (mixNode, nullptr);
    wrongType.removeChild (mixNode, nullptr);
    wrongType.addChild (disguisedMix, mixIndex, nullptr);
    expectRejected (wrongType,
                    "A node with id=mix and a non-PARAM type must be rejected");

    const float previousMix = restored.apvts.getRawParameterValue ("mix")->load();
    require (presetFile.replaceWithText ("<GateVerbState schemaVersion=\"1\"/>"),
             "Unable to write partial test preset");
    require (restored.loadUserPreset (presetFile).failed(), "A partial preset must be rejected");
    require (restored.apvts.getRawParameterValue ("mix")->load() == previousMix,
             "An invalid preset must not partially change the current sound");
    constexpr char broken[] = "not a JUCE state";
    restored.setStateInformation (broken, static_cast<int> (sizeof (broken)));
    require (restored.apvts.getRawParameterValue ("mix")->load() == previousMix,
             "An invalid host state must leave the sound intact");
    require (directory.deleteRecursively(), "Temporary test files could not be removed");
}

void testLayoutsAndBlocks()
{
    for (int channels : { 1, 2 })
    {
        GateVerbAudioProcessor processor;
        auto layout = processor.getBusesLayout();
        layout.inputBuses.set (0, channels == 1 ? juce::AudioChannelSet::mono()
                                               : juce::AudioChannelSet::stereo());
        layout.outputBuses.set (0, layout.inputBuses[0]);
        require (processor.setBusesLayout (layout), "Mono/stereo layout rejected");
        setActual (processor, "mix", 0.0f);
        setActual (processor, "input", 0.0f);
        setActual (processor, "output", 0.0f);
        auto zeroCurve = processor.getCurve();
        zeroCurve.fill (0.0f);
        processor.setCurve (zeroCurve);
        processor.prepareToPlay (48000.0, 32);
        juce::MidiBuffer midi;
        for (int samples : { 0, 1, 17, 511, 8193 })
        {
            juce::AudioBuffer<float> buffer (channels, samples);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < samples; ++i)
                    buffer.setSample (c, i, 0.25f * std::sin (0.13f * static_cast<float> (i + c)));
            juce::AudioBuffer<float> reference;
            reference.makeCopyOf (buffer);
            processor.processBlock (buffer, midi);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < samples; ++i)
                    require (std::abs (buffer.getSample (c, i) - reference.getSample (c, i)) < 1.0e-6f,
                             "Dry signal changed at a variable/oversized host block");
        }
        processor.releaseResources();
        processor.prepareToPlay (96000.0, 1024);
        juce::AudioBuffer<float> silence (channels, 1024);
        silence.clear();
        processor.processBlock (silence, midi);
        require (silence.getMagnitude (0, 1024) == 0.0f, "Re-prepare must not leak a stale tail");
        processor.releaseResources();
    }
}

void writeSnapshot (const juce::File& destination)
{
    GateVerbAudioProcessor processor;
    processor.loadFactoryPreset (0);
    processor.prepareToPlay (48000.0, 256);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, 256);
    // Feed an actual synthetic rhythmic signal, so the meters and playhead in
    // the snapshot are generated by the same engine used in the VST3.
    for (int block = 0; block < 217; ++block)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const int time = block * buffer.getNumSamples() + i;
            const auto envelope = std::exp (-static_cast<float> (time % 12000) / 2000.0f);
            buffer.setSample (0, i, 0.7f * envelope * std::sin (time * 0.035f));
            buffer.setSample (1, i, 0.6f * envelope * std::sin (time * 0.041f));
        }
        processor.processBlock (buffer, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    require (editor != nullptr, "The editor could not be created");
    editor->setVisible (true);
    const auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.5f);
    require (snapshot.isValid(), "The editor snapshot is empty");
    require (destination.getParentDirectory().createDirectory().wasOk(), "Unable to create screenshot directory");
    auto stream = destination.createOutputStream();
    require (stream != nullptr && stream->openedOk(), "Unable to open screenshot output");
    juce::PNGImageFormat png;
    require (png.writeImageToStream (snapshot, *stream), "Unable to encode editor screenshot");
    std::cout << "UI snapshot: " << destination.getFullPathName() << '\n';
    editor.reset();
    processor.releaseResources();
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    try
    {
        testHostChangeNotifications();
        testStateAndPresets();
        testLayoutsAndBlocks();
        if (argc == 3 && juce::String (argv[1]) == "--snapshot")
            writeSnapshot (juce::File (juce::String (argv[2])));
        std::cout << "PASS: host edit/recall notifications, 18-parameter/256-point state, "
                     "presets, corrupt-state/numeric rejection, "
                     "mono/stereo, zero/variable/oversized blocks, re-prepare\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
