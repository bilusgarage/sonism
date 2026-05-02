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

    void setOscParameters (int index, int type, float mix, float detune, float spread)
    {
        if (index >= 0 && index < 7)
        {
            waveformType[index] = type;
            this->mix[index] = mix;
            this->detune[index] = detune;
            this->spread[index] = spread;
        }
    }

    void setLfoParameters (int lfoIndex, float rate, float amount, float phaseOffset, int waveType)
    {
        if (lfoIndex == 1)
        {
            lfo1Rate = rate; lfo1Amount = amount; lfo1PhaseOffset = phaseOffset; lfo1Wave = waveType;
        }
        else if (lfoIndex == 2)
        {
            lfo2Rate = rate; lfo2Amount = amount; lfo2PhaseOffset = phaseOffset; lfo2Wave = waveType;
        }
    }

    void setAdsrParameters (float attack, float decay, float sustain, float release)
    {
        adsrParams.attack = attack;
        adsrParams.decay = decay;
        adsrParams.sustain = sustain;
        adsrParams.release = release;
        adsr.setParameters (adsrParams);
    }

    void setScopeBuffers (juce::AudioBuffer<float>* buffers[7])
    {
        for (int i = 0; i < 7; ++i)
            oscScopeBuffer[i] = buffers[i];
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        for (int i = 0; i < 7; ++i)
            currentAngle[i] = 0.0;
        lfo1Angle = 0.0f;
        lfo2Angle = 0.0f;
        level = velocity * 0.15;
        adsr.setSampleRate (getSampleRate());
        adsr.noteOn();
        currentPitchHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            clearCurrentNote();
            for (int i = 0; i < 7; ++i)
                angleDelta[i] = 0.0;
            currentPitchHz = 0.0;
            adsr.reset();
        }
    }

    void pitchWheelMoved (int /*newValue*/) override {}
    void controllerMoved (int /*controllerNumber*/, int /*newValue*/) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (currentPitchHz > 0.0)
        {
            float panL[7], panR[7];
            auto sampleRate = getSampleRate();
            float lfo1Delta = lfo1Rate / sampleRate;
            float lfo2Delta = lfo2Rate / sampleRate;

            for (int i = 0; i < 7; ++i)
            {
                panL[i] = std::cos ((spread[i] + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                panR[i] = std::sin ((spread[i] + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            }

            while (--numSamples >= 0)
            {
                // Advance LFO 1
                lfo1Angle += lfo1Delta;
                if (lfo1Angle >= 1.0f) lfo1Angle -= 1.0f;
                float phase1 = std::fmod (lfo1Angle + lfo1PhaseOffset, 1.0f);
                float lfo1Val = 0.0f;
                if (lfo1Wave == 0) lfo1Val = std::sin (phase1 * juce::MathConstants<float>::twoPi);
                else if (lfo1Wave == 1) lfo1Val = 2.0f * std::abs (2.0f * phase1 - 1.0f) - 1.0f;
                else if (lfo1Wave == 2) lfo1Val = phase1 < 0.5f ? 1.0f : -1.0f;
                else if (lfo1Wave == 3) lfo1Val = phase1 * 2.0f - 1.0f;
                else if (lfo1Wave == 4) lfo1Val = phase1 < 0.25f ? 1.0f : -1.0f;
                lfo1Val *= lfo1Amount * 100.0f;

                // Advance LFO 2
                lfo2Angle += lfo2Delta;
                if (lfo2Angle >= 1.0f) lfo2Angle -= 1.0f;
                float phase2 = std::fmod (lfo2Angle + lfo2PhaseOffset, 1.0f);
                float lfo2Val = 0.0f;
                if (lfo2Wave == 0) lfo2Val = std::sin (phase2 * juce::MathConstants<float>::twoPi);
                else if (lfo2Wave == 1) lfo2Val = 2.0f * std::abs (2.0f * phase2 - 1.0f) - 1.0f;
                else if (lfo2Wave == 2) lfo2Val = phase2 < 0.5f ? 1.0f : -1.0f;
                else if (lfo2Wave == 3) lfo2Val = phase2 * 2.0f - 1.0f;
                else if (lfo2Wave == 4) lfo2Val = phase2 < 0.25f ? 1.0f : -1.0f;
                lfo2Val *= lfo2Amount * 100.0f;

                auto envelope = (float) (level * adsr.getNextSample());
                float currentSampleL = 0.0f;
                float currentSampleR = 0.0f;

                for (int i = 0; i < 7; ++i)
                {
                    float currentDetune = detune[i];
                    if (i >= 1 && i <= 3) currentDetune += lfo1Val;
                    else if (i >= 4 && i <= 6) currentDetune += lfo2Val;

                    auto cyclesPerSample = (currentPitchHz * std::pow (2.0, currentDetune / 100.0)) / sampleRate;
                    double delta = cyclesPerSample * juce::MathConstants<double>::twoPi;
                    
                    auto sampleValue = getWaveformSample (waveformType[i], currentAngle[i]);
                    auto oscSampleL = sampleValue * mix[i] * envelope * panL[i];
                    auto oscSampleR = sampleValue * mix[i] * envelope * panR[i];

                    currentSampleL += oscSampleL;
                    currentSampleR += oscSampleR;

                    if (oscScopeBuffer[i] != nullptr && oscScopeBuffer[i]->getNumChannels() > 1)
                    {
                        oscScopeBuffer[i]->addSample (0, startSample, oscSampleL);
                        oscScopeBuffer[i]->addSample (1, startSample, oscSampleR);
                    }

                    currentAngle[i] += delta;
                    if (currentAngle[i] >= juce::MathConstants<double>::twoPi)
                        currentAngle[i] -= juce::MathConstants<double>::twoPi;
                }

                outputBuffer.addSample (0, startSample, currentSampleL);
                if (outputBuffer.getNumChannels() > 1)
                    outputBuffer.addSample (1, startSample, currentSampleR);

                ++startSample;

                if (! adsr.isActive())
                {
                    clearCurrentNote();
                    for (int i = 0; i < 7; ++i)
                        angleDelta[i] = 0.0;
                    currentPitchHz = 0.0;
                    break;
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

    double currentAngle[7] = { 0.0 };
    double angleDelta[7] = { 0.0 };
    double currentPitchHz = 0.0, level = 0.0;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    int waveformType[7] = { 0 };
    float mix[7] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float detune[7] = { 0.0f };
    float spread[7] = { 0.0f };
    
    float lfo1Rate = 1.0f, lfo1Amount = 0.0f, lfo1PhaseOffset = 0.0f, lfo1Angle = 0.0f;
    int lfo1Wave = 0;
    
    float lfo2Rate = 1.0f, lfo2Amount = 0.0f, lfo2PhaseOffset = 0.0f, lfo2Angle = 0.0f;
    int lfo2Wave = 0;
    juce::AudioBuffer<float>* oscScopeBuffer[7] = { nullptr };
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
    std::unique_ptr<AudioBufferFifo<float>> oscScopeFifos[7];

private:
    juce::Synthesiser synth;
    juce::AudioBuffer<float> oscScopeBuffers[7];
    juce::dsp::StateVariableTPTFilter<float> filters[2];
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
