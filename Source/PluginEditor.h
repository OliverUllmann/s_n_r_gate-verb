#pragma once

#include "PluginProcessor.h"
#include <memory>

class GateVerbLookAndFeel;
class GateVerbDial;
class GateVerbCurveEditor;
class GateVerbMeters;

/** Native, vector-drawn editor. The animation reads the audio engine's clock;
    its timer never drives the gate or any part of audio processing. */
class GateVerbAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::ChangeListener
{
public:
    explicit GateVerbAudioProcessorEditor (GateVerbAudioProcessor&);
    ~GateVerbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateControlStates();
    void refreshPresetName();
    void choosePreset (int direction);
    void savePreset();
    void loadPreset();
    void showHelp();
    void showPresetError (const juce::Result&);

    GateVerbAudioProcessor& processor;
    std::unique_ptr<GateVerbLookAndFeel> look;
    juce::Component canvas;
    std::unique_ptr<GateVerbCurveEditor> curve;
    std::unique_ptr<GateVerbMeters> meters;
    juce::OwnedArray<GateVerbDial> reverbDials;
    std::unique_ptr<GateVerbDial> inputDial, outputDial, mixDial;
    std::unique_ptr<GateVerbDial> rateDial, depthDial, smoothDial, phaseDial;

    juce::ComboBox presetBox, divisionBox, gridBox, shapeBox;
    juce::TextButton previousPreset { "<" }, nextPreset { ">" };
    juce::TextButton saveButton { "SAVE" }, loadButton { "LOAD" }, helpButton { "?" };
    juce::TextButton freeDrawButton { "DRAW" }, stepsButton { "STEPS" };
    juce::TextButton undoButton { "UNDO" }, redoButton { "REDO" };
    juce::TextButton syncButton { "HOST SYNC" }, gateButton { "GATE ON" };
    juce::Label clockLabel, modeLabel, versionLabel;
    juce::TooltipWindow tooltips { this, 650 };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, gateAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int lastFactoryPreset = 0;
    bool refreshing = false;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateVerbAudioProcessorEditor)
};
