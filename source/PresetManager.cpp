#include "PresetManager.h"

const juce::String PresetManager::extension = "preset";
const juce::String PresetManager::presetNameProperty = "presetName";

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState (apvts)
{
    // Create a default preset directory if it doesn't exist
    auto defaultDirectory = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                .getChildFile ("Presets")
                                .getChildFile ("bilusgarage")
                                .getChildFile ("Sonism");

    if (! defaultDirectory.exists())
    {
        defaultDirectory.createDirectory();
    }

    valueTreeState.state.addListener (this);
    currentPreset.referTo (valueTreeState.state.getPropertyAsValue (presetNameProperty, nullptr));
}

void PresetManager::savePreset (const juce::String& presetName)
{
    if (presetName.isEmpty())
        return;

    currentPreset.setValue (presetName);
    auto xml = valueTreeState.copyState().createXml();

    auto presetFile = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                          .getChildFile ("Presets")
                          .getChildFile ("bilusgarage")
                          .getChildFile ("Sonism")
                          .getChildFile (presetName + "." + extension);

    if (! presetFile.existsAsFile())
    {
        presetFile.create();
    }

    presetFile.replaceWithText (xml->toString());
}

void PresetManager::loadPreset (const juce::String& presetName)
{
    if (presetName.isEmpty())
        return;

    auto presetFile = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                          .getChildFile ("Presets")
                          .getChildFile ("bilusgarage")
                          .getChildFile ("Sonism")
                          .getChildFile (presetName + "." + extension);

    if (presetFile.existsAsFile())
    {
        juce::XmlDocument xmlDocument (presetFile);
        auto xmlState = xmlDocument.getDocumentElement();

        if (xmlState != nullptr)
        {
            auto vt = juce::ValueTree::fromXml (*xmlState);
            valueTreeState.replaceState (vt);
            currentPreset.setValue (presetName);
        }
    }
}

int PresetManager::loadNextPreset()
{
    auto allPresets = getAllPresets();
    if (allPresets.isEmpty())
        return -1;

    auto currentIndex = allPresets.indexOf (currentPreset.toString());
    auto nextIndex = currentIndex + 1;
    if (nextIndex >= allPresets.size())
        nextIndex = 0;

    loadPreset (allPresets[nextIndex]);
    return nextIndex;
}

int PresetManager::loadPreviousPreset()
{
    auto allPresets = getAllPresets();
    if (allPresets.isEmpty())
        return -1;

    auto currentIndex = allPresets.indexOf (currentPreset.toString());
    auto previousIndex = currentIndex - 1;
    if (previousIndex < 0)
        previousIndex = allPresets.size() - 1;

    loadPreset (allPresets[previousIndex]);
    return previousIndex;
}

juce::StringArray PresetManager::getAllPresets() const
{
    juce::StringArray presets;
    auto defaultDirectory = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                .getChildFile ("Presets")
                                .getChildFile ("bilusgarage")
                                .getChildFile ("Sonism");

    auto presetFiles = defaultDirectory.findChildFiles (juce::File::TypesOfFileToFind::findFiles, false, "*." + extension);
    for (auto& file : presetFiles)
    {
        presets.add (file.getFileNameWithoutExtension());
    }

    return presets;
}

juce::String PresetManager::getCurrentPreset() const
{
    return currentPreset.toString();
}

void PresetManager::valueTreeRedirected (juce::ValueTree& treeWhichHasBeenChanged)
{
    juce::ignoreUnused (treeWhichHasBeenChanged);
    currentPreset.referTo (valueTreeState.state.getPropertyAsValue (presetNameProperty, nullptr));
}
