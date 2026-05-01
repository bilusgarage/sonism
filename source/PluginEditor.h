#pragma once

#include "BinaryData.h"
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
class SpectrumDisplayComponent : public juce::Component
{
public:
    SpectrumDisplayComponent()
        : forwardFFT (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        setOpaque (true);
        juce::zeromem (fifo, sizeof (fifo));
        juce::zeromem (fftData, sizeof (fftData));
        juce::zeromem (scopeData, sizeof (scopeData));
    }

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
    }

    void pushBuffer (const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() == 0)
            return;

        auto* channelData = buffer.getReadPointer (0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (fifoIndex == fftSize)
            {
                if (! nextFFTBlockReady)
                {
                    juce::zeromem (fftData, sizeof (fftData));
                    memcpy (fftData, fifo, sizeof (fifo));
                    nextFFTBlockReady = true;
                }
                fifoIndex = 0;
            }
            fifo[fifoIndex++] = channelData[i];
        }

        if (nextFFTBlockReady)
        {
            window.multiplyWithWindowingTable (fftData, fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform (fftData);

            for (int i = 0; i < scopeSize; ++i)
            {
                auto skewedProportionX = 1.0f - std::exp (std::log (1.0f - (float) i / (float) scopeSize) * 0.2f);
                auto fftDataIndex = juce::jlimit (0, fftSize / 2, (int) (skewedProportionX * (float) fftSize * 0.5f));
                auto level = juce::jmap (juce::Decibels::gainToDecibels (fftData[fftDataIndex])
                                                           - juce::Decibels::gainToDecibels ((float) fftSize),
                                         mindB, maxdB, 0.0f, 1.0f);

                scopeData[i] = level;
            }

            nextFFTBlockReady = false;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll (juce::Colours::black);

        drawScales (g, bounds);

        g.setColour (juce::Colours::cyan);

        juce::Path p;
        for (int i = 0; i < scopeSize; ++i)
        {
            auto x = juce::jmap ((float) i, 0.0f, (float) scopeSize - 1.0f, bounds.getX(), bounds.getRight());
            auto y = juce::jmap (scopeData[i], 0.0f, 1.0f, bounds.getBottom(), bounds.getY());

            if (i == 0)
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }

        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

    void drawScales (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (juce::Colours::darkgrey);
        g.setFont (10.0f);

        // draw y-axis (dB)
        for (float db = maxdB; db >= mindB; db -= 20.0f)
        {
            auto y = juce::jmap (db, mindB, maxdB, bounds.getBottom(), bounds.getY());
            g.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
            
            float yOffset = (db >= maxdB - 0.1f) ? 2.0f : -12.0f; 
            g.drawText (juce::String ((int) db) + " dB", 
                        juce::Rectangle<float>(bounds.getRight() - 40, y + yOffset, 40, 10), 
                        juce::Justification::topRight);
        }

        // draw x-axis (frequency)
        if (sampleRate > 0.0)
        {
            std::vector<float> freqs = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
            for (auto f : freqs)
            {
                auto nyquist = sampleRate / 2.0;
                if (f > nyquist) continue;

                auto s = (float) f / (float) nyquist;
                auto proportion = 1.0f - std::pow (1.0f - s, 5.0f);
                auto x = proportion * bounds.getWidth() + bounds.getX();

                g.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());

                juce::String label;
                if (f >= 1000) label = juce::String (f / 1000, 0) + "k";
                else label = juce::String (f, 0);

                g.drawText (label, 
                            juce::Rectangle<float>(x + 2, bounds.getBottom() - 12, 30, 10), 
                            juce::Justification::bottomLeft);
            }
        }
    }

private:
    static constexpr auto fftOrder = 11;
    static constexpr auto fftSize  = 1 << fftOrder;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fifo [fftSize];
    float fftData [fftSize * 2];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    static constexpr auto scopeSize = 512;
    float scopeData [scopeSize];

    double sampleRate = 44100.0;
    static constexpr float mindB = -100.0f;
    static constexpr float maxdB = 0.0f;
};

//==============================================================================
class WaveformDisplayComponent : public juce::Component
{
public:
    WaveformDisplayComponent()
    {
        samples.resize (512, 0.0f);
    }

    void pushBuffer (const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() == 0)
            return;

        auto* channelData = buffer.getReadPointer (0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            samples[(size_t) writePosition] = juce::jlimit (-1.0f, 1.0f, channelData[i] * displayGain);
            writePosition = (writePosition + 1) % (int) samples.size();
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (juce::Colours::black);
        g.fillRect (bounds);

        g.setColour (juce::Colours::darkgrey);
        auto centerY = bounds.getCentreY();
        g.drawHorizontalLine (juce::roundToInt (centerY), bounds.getX(), bounds.getRight());

        g.setColour (juce::Colours::cyan);
        juce::Path p;

        for (size_t i = 0; i < samples.size(); ++i)
        {
            auto sampleIndex = ((size_t) writePosition + i) % samples.size();
            auto x = juce::jmap ((float) i, 0.0f, (float) samples.size() - 1.0f, bounds.getX(), bounds.getRight());
            auto y = centerY - (samples[sampleIndex] * bounds.getHeight() * 0.45f);

            if (i == 0)
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }

        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

private:
    std::vector<float> samples;
    int writePosition = 0;
    float displayGain = 1.0f;
};

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;

    SpectrumDisplayComponent outputVisualiser;
    juce::AudioBuffer<float> tempScopeBuffer;

    juce::MidiKeyboardComponent keyboardComponent;
    juce::GroupComponent osc1Group;
    WaveformDisplayComponent waveform1Display;
    juce::ComboBox waveform1Selector;
    juce::Slider osc1MixSlider;
    juce::GroupComponent osc2Group;
    WaveformDisplayComponent waveform2Display;
    juce::ComboBox waveform2Selector;
    juce::Slider osc2MixSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osc1WaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osc2WaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> osc1MixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> osc2MixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
