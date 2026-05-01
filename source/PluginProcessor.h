#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#if (MSVC)
    #include "ipps.h"
#endif

//==============================================================================
template <typename Type>
class AudioBufferFifo
{
public:
    AudioBufferFifo (int capacityInSamples) : fifo (capacityInSamples) {}

    void prepare (int numChannels)
    {
        buffer.setSize (numChannels, fifo.getTotalSize(), false, true, true);
        buffer.clear();
        fifo.reset();
    }

    void push (const juce::AudioBuffer<Type>& data)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (data.getNumSamples(), start1, size1, start2, size2);

        if (size1 > 0)
            for (int ch = 0; ch < std::min (buffer.getNumChannels(), data.getNumChannels()); ++ch)
                buffer.copyFrom (ch, start1, data, ch, 0, size1);

        if (size2 > 0)
            for (int ch = 0; ch < std::min (buffer.getNumChannels(), data.getNumChannels()); ++ch)
                buffer.copyFrom (ch, start2, data, ch, size1, size2);

        fifo.finishedWrite (size1 + size2);
    }

    void pop (juce::AudioBuffer<Type>& data)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (data.getNumSamples(), start1, size1, start2, size2);

        if (size1 > 0)
            for (int ch = 0; ch < std::min (buffer.getNumChannels(), data.getNumChannels()); ++ch)
                data.copyFrom (ch, 0, buffer, ch, start1, size1);

        if (size2 > 0)
            for (int ch = 0; ch < std::min (buffer.getNumChannels(), data.getNumChannels()); ++ch)
                data.copyFrom (ch, size1, buffer, ch, start2, size2);

        fifo.finishedRead (size1 + size2);
    }

    int getNumReady() const { return fifo.getNumReady(); }

private:
    juce::AbstractFifo fifo;
    juce::AudioBuffer<Type> buffer;
};

//==============================================================================
class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel (int /*midiChannel*/) override { return true; }
};

//==============================================================================
class SynthVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SynthSound*> (sound) != nullptr;
    }

    void setOsc1Parameters (int type, float mix)
    {
        waveformType1 = type;
        mix1 = mix;
    }

    void setOsc2Parameters (int type, float mix)
    {
        waveformType2 = type;
        mix2 = mix;
    }

    void setScopeBuffers (juce::AudioBuffer<float>* osc1BufferToUse, juce::AudioBuffer<float>* osc2BufferToUse)
    {
        osc1ScopeBuffer = osc1BufferToUse;
        osc2ScopeBuffer = osc2BufferToUse;
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (juce::approximatelyEqual (tailOff, 0.0))
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved (int /*newValue*/) override {}
    void controllerMoved (int /*controllerNumber*/, int /*newValue*/) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (!juce::approximatelyEqual (angleDelta, 0.0))
        {
            while (--numSamples >= 0)
            {
                auto sampleValue1 = getWaveformSample (waveformType1, currentAngle);
                auto sampleValue2 = getWaveformSample (waveformType2, currentAngle);

                float mixedSample = (sampleValue1 * mix1) + (sampleValue2 * mix2);
                auto envelope = (float) (level * (tailOff > 0.0 ? tailOff : 1.0f));
                auto osc1Sample = sampleValue1 * envelope;
                auto osc2Sample = sampleValue2 * envelope;
                auto currentSample = (mixedSample * envelope);

                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample (i, startSample, currentSample);

                if (osc1ScopeBuffer != nullptr)
                    osc1ScopeBuffer->addSample (0, startSample, osc1Sample);

                if (osc2ScopeBuffer != nullptr)
                    osc2ScopeBuffer->addSample (0, startSample, osc2Sample);

                currentAngle += angleDelta;
                ++startSample;

                if (tailOff > 0.0)
                {
                    tailOff *= 0.99;
                    if (tailOff <= 0.005)
                    {
                        clearCurrentNote();
                        angleDelta = 0.0;
                        break;
                    }
                }
            }
        }
    }

private:
    static float getWaveformSample (int waveformType, double angle)
    {
        auto phase = angle / juce::MathConstants<double>::twoPi;
        auto normalisedPhase = phase - std::floor (phase);

        if (waveformType == 0) // Sine
            return (float) std::sin (angle);

        if (waveformType == 1) // Triangle
            return (float) (2.0 * std::abs (2.0 * (normalisedPhase - 0.5)) - 1.0);

        if (waveformType == 2) // Square
            return normalisedPhase < 0.5 ? 1.0f : -1.0f;

        if (waveformType == 3) // Sawtooth
            return (float) ((normalisedPhase * 2.0) - 1.0);

        if (waveformType == 4) // Pulse
            return normalisedPhase < 0.25 ? 1.0f : -1.0f;

        return 0.0f;
    }

    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
    int waveformType1 = 0, waveformType2 = 0; // 0: sine, 1: triangle, 2: square, 3: sawtooth, 4: pulse
    float mix1 = 1.0f, mix2 = 0.0f;
    juce::AudioBuffer<float>* osc1ScopeBuffer = nullptr;
    juce::AudioBuffer<float>* osc2ScopeBuffer = nullptr;
};

//==============================================================================
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::MidiKeyboardState keyboardState;
    juce::AudioProcessorValueTreeState apvts;

    AudioBufferFifo<float> scopeFifo { 48000 };
    AudioBufferFifo<float> osc1ScopeFifo { 48000 };
    AudioBufferFifo<float> osc2ScopeFifo { 48000 };

private:
    juce::Synthesiser synth;
    juce::AudioBuffer<float> osc1ScopeBuffer;
    juce::AudioBuffer<float> osc2ScopeBuffer;
    juce::dsp::StateVariableTPTFilter<float> filters[2];
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
