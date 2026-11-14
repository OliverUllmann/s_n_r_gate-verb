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
        testStateAndPresets();
        testLayoutsAndBlocks();
        if (argc == 3 && juce::String (argv[1]) == "--snapshot")
            writeSnapshot (juce::File (juce::String (argv[2])));
        std::cout << "PASS: 18-parameter/256-point state, presets, corrupt-state rejection, "
                     "mono/stereo, zero/variable/oversized blocks, re-prepare\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
