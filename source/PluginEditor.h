#pragma once

#include "BinaryData.h"
#include "PluginProcessor.h"
//#include "melatonin_inspector/melatonin_inspector.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

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

        auto* channelDataL = buffer.getReadPointer (0);
        auto* channelDataR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : channelDataL;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (fifoIndex == fftSize)
            {
                if (!nextFFTBlockReady)
                {
                    juce::zeromem (fftData, sizeof (fftData));
                    memcpy (fftData, fifo, sizeof (fifo));
                    nextFFTBlockReady = true;
                }
                fifoIndex = 0;
            }
            fifo[fifoIndex++] = (channelDataL[i] + channelDataR[i]) * 0.5f;
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
                    mindB,
                    maxdB,
                    0.0f,
                    1.0f);

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
                juce::Rectangle<float> (bounds.getRight() - 40, y + yOffset, 40, 10),
                juce::Justification::topRight);
        }

        // draw x-axis (frequency)
        if (sampleRate > 0.0)
        {
            std::vector<float> freqs = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
            for (auto f : freqs)
            {
                auto nyquist = sampleRate / 2.0;
                if (f > nyquist)
                    continue;

                auto s = (float) f / (float) nyquist;
                auto proportion = 1.0f - std::pow (1.0f - s, 5.0f);
                auto x = proportion * bounds.getWidth() + bounds.getX();

                g.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());

                juce::String label;
                if (f >= 1000)
                    label = juce::String (f / 1000, 0) + "k";
                else
                    label = juce::String (f, 0);

                g.drawText (label,
                    juce::Rectangle<float> (x + 2, bounds.getBottom() - 12, 30, 10),
                    juce::Justification::bottomLeft);
            }
        }
    }

private:
    static constexpr auto fftOrder = 11;
    static constexpr auto fftSize = 1 << fftOrder;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fifo[fftSize];
    float fftData[fftSize * 2];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    static constexpr auto scopeSize = 512;
    float scopeData[scopeSize];

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

    void setColour (juce::Colour newColour)
    {
        waveColour = newColour;
        repaint();
    }

    void pushBuffer (const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() == 0)
            return;

        auto* channelDataL = buffer.getReadPointer (0);
        auto* channelDataR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : channelDataL;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float summed = (channelDataL[i] + channelDataR[i]) * 0.5f;
            samples[(size_t) writePosition] = juce::jlimit (-1.0f, 1.0f, summed * displayGain);
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

        g.setColour (waveColour);
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
    juce::Colour waveColour = juce::Colours::cyan;
};

//==============================================================================
class LfoDisplayComponent : public juce::Component
{
public:
    LfoDisplayComponent() {}

    void setParameters (int waveform, float phase, float amount)
    {
        currentWaveform = waveform;
        currentPhase = phase;
        currentAmount = amount;
        repaint();
    }

    void setColour (juce::Colour newColour)
    {
        waveColour = newColour;
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

        g.setColour (waveColour);
        juce::Path p;
        int numPoints = 200;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float xProportion = (float) i / (numPoints - 1);
            float x = bounds.getX() + xProportion * bounds.getWidth();
            
            // Generate basic wave shape for visualization
            float phaseVal = std::fmod (xProportion * 2.0f + currentPhase, 1.0f);
            float yVal = 0.0f;
            
            if (currentWaveform == 0) // Sine
                yVal = std::sin (phaseVal * juce::MathConstants<float>::twoPi);
            else if (currentWaveform == 1) // Saw
                yVal = phaseVal * 2.0f - 1.0f;
            else if (currentWaveform == 2) // Triangle
                yVal = 2.0f * std::abs (2.0f * phaseVal - 1.0f) - 1.0f;

            yVal *= currentAmount;
            
            float y = centerY - (yVal * bounds.getHeight() * 0.45f);

            if (i == 0)
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }

        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

private:
    int currentWaveform = 0; // 0=Sine, 1=Saw, 2=Tri
    float currentPhase = 0.0f;
    float currentAmount = 1.0f;
    juce::Colour waveColour = juce::Colours::yellow;
};

//==============================================================================
class EffectTileComponent : public juce::Component
{
public:
    EffectTileComponent (const juce::String& name) : effectName (name) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.setColour (juce::Colour (0xff2a2a2a));
        g.fillRoundedRectangle (bounds, 5.0f);
        
        // Border
        g.setColour (juce::Colour (0xff4a4a4a));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
        
        // Text
        g.setColour (juce::Colours::white);
        g.setFont (18.0f);
        g.drawText (effectName, getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::String effectName;
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
    void mouseDown (const juce::MouseEvent& e) override;

private:
    void updateOscTabs();
    void updateLfoTabs();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    //std::unique_ptr<melatonin::Inspector> inspector;

    SpectrumDisplayComponent outputVisualiser;
    juce::AudioBuffer<float> tempScopeBuffer;

    juce::MidiKeyboardComponent keyboardComponent;

    juce::GroupComponent oscGroup;
    std::array<juce::TextButton, 7> oscTabButtons;

    std::array<WaveformDisplayComponent, 7> waveformDisplays;
    std::array<juce::ComboBox, 7> waveformSelectors;
    std::array<juce::Slider, 7> oscMixSliders;
    std::array<juce::Slider, 7> oscDetuneSliders; // index 0 unused
    std::array<juce::Label, 7> oscDetuneLabels;
    std::array<juce::Slider, 7> oscPanSliders;
    std::array<juce::Label, 7> oscPanLabels;

    juce::Slider unisonDetuneSlider;
    juce::Label unisonDetuneLabel;
    juce::Slider unisonSpreadSlider;
    juce::Label unisonSpreadLabel;

    juce::GroupComponent filterGroup;
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;
    juce::Label cutoffLabel;
    juce::Label resonanceLabel;

    juce::GroupComponent ampEnvGroup;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;

    // LFO Section
    juce::GroupComponent lfoGroup { "LFO", "LFO" };
    juce::TextButton lfoTab1 { "LFO 2-4" };
    juce::TextButton lfoTab2 { "LFO 5-7" };
    
    std::array<juce::ShapeButton, 2> lfoSineButton {{
        juce::ShapeButton { "Sine", juce::Colours::darkgrey, juce::Colours::purple.withAlpha(0.5f), juce::Colours::purple },
        juce::ShapeButton { "Sine", juce::Colours::darkgrey, juce::Colours::green.withAlpha(0.5f), juce::Colours::green }
    }};
    std::array<juce::ShapeButton, 2> lfoSawButton {{
        juce::ShapeButton { "Saw", juce::Colours::darkgrey, juce::Colours::purple.withAlpha(0.5f), juce::Colours::purple },
        juce::ShapeButton { "Saw", juce::Colours::darkgrey, juce::Colours::green.withAlpha(0.5f), juce::Colours::green }
    }};
    std::array<juce::ShapeButton, 2> lfoTriangleButton {{
        juce::ShapeButton { "Triangle", juce::Colours::darkgrey, juce::Colours::purple.withAlpha(0.5f), juce::Colours::purple },
        juce::ShapeButton { "Triangle", juce::Colours::darkgrey, juce::Colours::green.withAlpha(0.5f), juce::Colours::green }
    }};
    
    std::array<juce::TextButton, 2> lfoSyncButton {{ juce::TextButton { "Hz" }, juce::TextButton { "Hz" } }};
    
    std::array<juce::Slider, 2> lfoAmountSlider;
    std::array<juce::Slider, 2> lfoRateSlider;
    std::array<juce::Slider, 2> lfoPhaseSlider;
    std::array<juce::Label, 2> lfoAmountLabel;
    std::array<juce::Label, 2> lfoRateLabel;
    std::array<juce::Label, 2> lfoPhaseLabel;
    
    LfoDisplayComponent lfoDisplay;

    // Effects Section
    juce::GroupComponent effectsGroup { "Effects", "Effects" };
    EffectTileComponent reverbTile { "REVERB" };
    EffectTileComponent delayTile { "DELAY" };
    EffectTileComponent compressorTile { "COMPRESSOR" };

    // Preset Header Section
    juce::Label pluginNameLabel;
    juce::TextButton presetButton { "Default Preset" };
    juce::TextButton presetLeftButton { "<" };
    juce::TextButton presetRightButton { ">" };

    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 7> oscWaveAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> oscMixAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> oscDetuneAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> oscPanAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonDetuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonSpreadAttachment;

    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2> lfoAmountAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2> lfoRateAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2> lfoPhaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
