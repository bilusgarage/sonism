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

    // Initialize per-oscillator scope FIFOs
    for (int i = 0; i < 7; ++i)
        oscScopeFifos[i] = std::make_unique<AudioBufferFifo<float>> (48000);
}

PluginProcessor::~PluginProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray waveTypes = { "Sine", "Triangle", "Square", "Sawtooth", "Pulse" };
    for (int i = 1; i <= 7; ++i)
    {
        juce::String id = juce::String (i);
        layout.add (std::make_unique<juce::AudioParameterChoice> ("OSC" + id + "WAVETYPE", "Osc " + id + " Wave Type", waveTypes, 3));
        layout.add (std::make_unique<juce::AudioParameterFloat> ("OSC" + id + "MIX",    "Osc " + id + " Mix",    0.0f, 1.0f, i == 1 ? 1.0f : 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> ("OSC" + id + "SPREAD", "Osc " + id + " Spread", -1.0f, 1.0f, 0.0f));
        if (i > 1)
            layout.add (std::make_unique<juce::AudioParameterFloat> ("OSC" + id + "DETUNE", "Osc " + id + " Detune", -100.0f, 100.0f, 0.0f));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> ("FILTERCUTOFF", "Cutoff", juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("FILTERRES", "Resonance", juce::NormalisableRange<float> (0.1f, 1.0f, 0.01f), 0.1f));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("ATTACK",  "Attack",  juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.5f), 0.1f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("DECAY",   "Decay",   juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.5f), 0.1f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("SUSTAIN", "Sustain", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("RELEASE", "Release", juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.5f), 0.1f));

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
    scopeFifo.prepare (juce::jmax (2, getTotalNumOutputChannels()));
    for (int i = 0; i < 7; ++i)
    {
        oscScopeFifos[i]->prepare (2);
        oscScopeBuffers[i].setSize (2, samplesPerBlock);
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    for (auto& f : filters)
    {
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }
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
    float oscWaveType[7], oscMix[7], oscDetune[7], oscSpread[7];
    for (int i = 0; i < 7; ++i)
    {
        juce::String idStr = juce::String (i + 1);
        oscWaveType[i] = apvts.getRawParameterValue ("OSC" + idStr + "WAVETYPE")->load();
        oscMix[i] = apvts.getRawParameterValue ("OSC" + idStr + "MIX")->load();
        oscSpread[i] = apvts.getRawParameterValue ("OSC" + idStr + "SPREAD")->load();
        oscDetune[i] = (i > 0) ? apvts.getRawParameterValue ("OSC" + idStr + "DETUNE")->load() : 0.0f;
    }

    auto attack = apvts.getRawParameterValue ("ATTACK")->load();
    auto decay = apvts.getRawParameterValue ("DECAY")->load();
    auto sustain = apvts.getRawParameterValue ("SUSTAIN")->load();
    auto release = apvts.getRawParameterValue ("RELEASE")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
        {
            for (int osc = 0; osc < 7; ++osc)
                voice->setOscParameters (osc, static_cast<int> (oscWaveType[osc]), oscMix[osc], oscDetune[osc], oscSpread[osc]);
            voice->setAdsrParameters (attack, decay, sustain, release);
        }
    }

    // Process the keyboard state to generate midi messages from our UI
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    // Resize + clear scope buffers, pass to voices
    juce::AudioBuffer<float>* buffers[7];
    for (int i = 0; i < 7; ++i)
    {
        oscScopeBuffers[i].setSize (2, buffer.getNumSamples(), false, false, true);
        oscScopeBuffers[i].clear();
        buffers[i] = &oscScopeBuffers[i];
    }

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->setScopeBuffers (buffers);

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    juce::AudioBuffer<float>* nullBuffers[7] = { nullptr };
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->setScopeBuffers (nullBuffers);

    auto cutoff = apvts.getRawParameterValue ("FILTERCUTOFF")->load();
    auto res = apvts.getRawParameterValue ("FILTERRES")->load();

    auto nyquist = getSampleRate() > 0.0 ? getSampleRate() * 0.5 : 22050.0;
    auto safeCutoff = juce::jlimit (20.0, nyquist - 1.0, (double) cutoff);

    juce::dsp::AudioBlock<float> fullBlock (buffer);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        if (ch < 2)
        {
            filters[ch].setCutoffFrequency ((float) safeCutoff);
            filters[ch].setResonance (juce::jmap (res, 0.1f, 1.0f, 0.707f, 5.0f)); // map 0.1-1.0 to reasonable resonance (Q) values
            auto singleChannelBlock = fullBlock.getSingleChannelBlock (ch);
            juce::dsp::ProcessContextReplacing<float> context (singleChannelBlock);
            filters[ch].process (context);
        }
    }

    scopeFifo.push (buffer);
    for (int i = 0; i < 7; ++i)
        oscScopeFifos[i]->push (oscScopeBuffers[i]);
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
