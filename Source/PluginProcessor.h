#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DspEngine.h"
#include <array>
#include <atomic>
#include <cstdint>

class GateVerbAudioProcessor final : public juce::AudioProcessor,
                                     public juce::ChangeBroadcaster,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr size_t curveSize = 256;
    using Curve = std::array<float, curveSize>;

    GateVerbAudioProcessor();
    ~GateVerbAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    Curve getCurve() const;
    void setCurve (const Curve&);
    void applyShape (int index);
    static Curve makeShape (int index);

    void loadFactoryPreset (int index);
    juce::StringArray getFactoryPresetNames() const;
    juce::String getPresetName() const;
    bool isPresetDirty() const { return presetDirty.load (std::memory_order_relaxed); }
    juce::Result saveUserPreset (const juce::File&);
    juce::Result loadUserPreset (const juce::File&);
    static juce::File getUserPresetDirectory();
    int getEditorMode() const { return editorMode.load (std::memory_order_relaxed); }
    void setEditorMode (int mode);
    int getGridSize() const { return gridSize.load (std::memory_order_relaxed); }
    void setGridSize (int size);

    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f }, meterWet { 0.0f };
    std::atomic<float> visualPhase { 0.0f }, visualGate { 1.0f };
    std::atomic<double> visualBpm { 120.0 };
    std::atomic<bool> visualPlaying { false }, visualHasHost { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String&, float) override;
    void markChanged();
    void publishCurve (const Curve&);
    void readCurveForAudio() noexcept;
    juce::ValueTree makeState();
    juce::Result restoreState (const juce::ValueTree&);
    void setParameter (const juce::String& id, float value);
    gateverb::DspEngine::Parameters readParameters() const noexcept;

    // Only writers and the UI take this lock. The audio callback never takes it.
    mutable juce::CriticalSection stateLock;
    std::array<std::atomic<float>, curveSize> curveStorage;
    std::atomic<uint32_t> curveRevision { 0 };
    uint32_t audioCurveRevision = UINT32_MAX;
    Curve audioCurve {};
    juce::String presetName { "01 - Gated Bloom" };
    std::atomic<bool> presetDirty { false };
    std::atomic<int> editorMode { 0 }, gridSize { 16 };
    std::array<std::atomic<float>*, 18> parameterValues {};
    gateverb::DspEngine engine;
    juce::AudioBuffer<float> monoScratch;
    double processingSampleRate = 44100.0;
    int scratchSize = 512;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateVerbAudioProcessor)
};
