#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager : public juce::ValueTree::Listener
{
public:
    static const juce::String extension;
    static const juce::String presetNameProperty;

    PresetManager (juce::AudioProcessorValueTreeState& apvts);

    void savePreset (const juce::String& presetName);
    void loadPreset (const juce::String& presetName);
    int loadNextPreset();
    int loadPreviousPreset();
    juce::StringArray getAllPresets() const;
    juce::String getCurrentPreset() const;

private:
    void valueTreeRedirected (juce::ValueTree& treeWhichHasBeenChanged) override;

    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Value currentPreset;
};
