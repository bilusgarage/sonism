#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
#if !JucePlugin_IsMidiEffect
    #if !JucePlugin_IsSynth
              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
    #endif
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Add 8 voices to our synth for polyphony
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new SynthVoice());

    // Add the sound that the voices can play
    synth.addSound (new SynthSound());
}

PluginProcessor::~PluginProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray waveTypes = { "Sine", "Triangle", "Square", "Sawtooth", "Pulse" };
    layout.add (std::make_unique<juce::AudioParameterChoice> ("OSC1WAVETYPE", "Osc 1 Wave Type", waveTypes, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("OSC2WAVETYPE", "Osc 2 Wave Type", waveTypes, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("OSC1MIX", "Osc 1 Mix", 0.0f, 1.0f, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("OSC2MIX", "Osc 2 Mix", 0.0f, 1.0f, 0.0f));
    return layout;
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return "Sonism";
}

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (samplesPerBlock);
    synth.setCurrentPlaybackSampleRate (sampleRate);
    scopeFifo.prepare (juce::jmax (1, getTotalNumOutputChannels()));
    osc1ScopeFifo.prepare (1);
    osc2ScopeFifo.prepare (1);
    osc1ScopeBuffer.setSize (1, samplesPerBlock);
    osc2ScopeBuffer.setSize (1, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
#endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Since we are synthesizing audio, we must clear all output channels so that
    // incoming audio (if any) doesn't interfere with our synthesized sound.
    for (auto i = 0; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Update voice parameters from APVTS
    auto osc1WaveType = apvts.getRawParameterValue ("OSC1WAVETYPE")->load();
    auto osc2WaveType = apvts.getRawParameterValue ("OSC2WAVETYPE")->load();
    auto osc1Mix = apvts.getRawParameterValue ("OSC1MIX")->load();
    auto osc2Mix = apvts.getRawParameterValue ("OSC2MIX")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
        {
            voice->setOsc1Parameters (static_cast<int> (osc1WaveType), osc1Mix);
            voice->setOsc2Parameters (static_cast<int> (osc2WaveType), osc2Mix);
        }
    }

    // Process the keyboard state to generate midi messages from our UI
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    // The synth processes the incoming midi messages and renders audio directly
    // into the buffer we provide.
    osc1ScopeBuffer.setSize (1, buffer.getNumSamples(), false, false, true);
    osc2ScopeBuffer.setSize (1, buffer.getNumSamples(), false, false, true);
    osc1ScopeBuffer.clear();
    osc2ScopeBuffer.clear();

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->setScopeBuffers (&osc1ScopeBuffer, &osc2ScopeBuffer);

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->setScopeBuffers (nullptr, nullptr);

    scopeFifo.push (buffer);
    osc1ScopeFifo.push (osc1ScopeBuffer);
    osc2ScopeFifo.push (osc2ScopeBuffer);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
