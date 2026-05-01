#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), keyboardComponent (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (outputVisualiser);
    addAndMakeVisible (keyboardComponent);

    oscGroup.setText ("");
    addAndMakeVisible (oscGroup);

    // Set up 7 tab buttons
    static const char* tabNames[7] = { "OSC 1", "OSC 2", "OSC 3", "OSC 4", "OSC 5", "OSC 6", "OSC 7" };
    for (int i = 0; i < 7; ++i)
    {
        auto& btn = oscTabButtons[(size_t) i];
        btn.setButtonText (tabNames[i]);
        btn.setRadioGroupId (1);
        btn.setClickingTogglesState (true);
        btn.setToggleState (i == 0, juce::dontSendNotification);
        btn.onClick = [this] { updateOscTabs(); };
        addAndMakeVisible (btn);
    }

    // Set up 7 oscillator controls
    juce::StringArray waveItems { "Sine", "Triangle", "Square", "Sawtooth", "Pulse" };
    for (int i = 0; i < 7; ++i)
    {
        juce::String idStr = juce::String (i + 1);

        // Waveform display
        addChildComponent (waveformDisplays[i]);

        // Wave selector
        waveformSelectors[i].addItemList (waveItems, 1);
        waveformSelectors[i].setSelectedId (4); // default Sawtooth
        addChildComponent (waveformSelectors[i]);

        // Mix slider
        oscMixSliders[i].setSliderStyle (juce::Slider::LinearHorizontal);
        oscMixSliders[i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addChildComponent (oscMixSliders[i]);

        // Pan slider + label
        oscPanSliders[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        oscPanSliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
        addChildComponent (oscPanSliders[i]);

        oscPanLabels[i].setText ("Pan", juce::dontSendNotification);
        oscPanLabels[i].attachToComponent (&oscPanSliders[i], false);
        oscPanLabels[i].setJustificationType (juce::Justification::centred);

        // Detune slider + label (not for OSC 1)
        if (i > 0)
        {
            oscDetuneSliders[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            oscDetuneSliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
            addChildComponent (oscDetuneSliders[i]);

            oscDetuneLabels[i].setText ("Detune", juce::dontSendNotification);
            oscDetuneLabels[i].attachToComponent (&oscDetuneSliders[i], false);
            oscDetuneLabels[i].setJustificationType (juce::Justification::centred);
        }

        // APVTS attachments
        oscWaveAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.apvts, "OSC" + idStr + "WAVETYPE", waveformSelectors[i]);
        oscMixAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "OSC" + idStr + "MIX", oscMixSliders[i]);
        oscPanAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "OSC" + idStr + "SPREAD", oscPanSliders[i]);
        if (i > 0)
            oscDetuneAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "OSC" + idStr + "DETUNE", oscDetuneSliders[i]);
    }

    // Filter group
    filterGroup.setText ("Filter");
    addAndMakeVisible (filterGroup);

    cutoffSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    cutoffSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible (cutoffSlider);

    resonanceSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    resonanceSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible (resonanceSlider);

    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "FILTERCUTOFF", cutoffSlider);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "FILTERRES", resonanceSlider);

    cutoffLabel.setText ("Cutoff", juce::dontSendNotification);
    cutoffLabel.attachToComponent (&cutoffSlider, false);
    cutoffLabel.setJustificationType (juce::Justification::centred);

    resonanceLabel.setText ("Res", juce::dontSendNotification);
    resonanceLabel.attachToComponent (&resonanceSlider, false);
    resonanceLabel.setJustificationType (juce::Justification::centred);

    // Amp Env group
    ampEnvGroup.setText ("Amp Env");
    addAndMakeVisible (ampEnvGroup);

    attackSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    attackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 20);
    addAndMakeVisible (attackSlider);

    decaySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    decaySlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 20);
    addAndMakeVisible (decaySlider);

    sustainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    sustainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 20);
    addAndMakeVisible (sustainSlider);

    releaseSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    releaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 20);
    addAndMakeVisible (releaseSlider);

    attackLabel.setText ("A", juce::dontSendNotification);
    attackLabel.attachToComponent (&attackSlider, false);
    attackLabel.setJustificationType (juce::Justification::centred);

    decayLabel.setText ("D", juce::dontSendNotification);
    decayLabel.attachToComponent (&decaySlider, false);
    decayLabel.setJustificationType (juce::Justification::centred);

    sustainLabel.setText ("S", juce::dontSendNotification);
    sustainLabel.attachToComponent (&sustainSlider, false);
    sustainLabel.setJustificationType (juce::Justification::centred);

    releaseLabel.setText ("R", juce::dontSendNotification);
    releaseLabel.attachToComponent (&releaseSlider, false);
    releaseLabel.setJustificationType (juce::Justification::centred);

    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "ATTACK", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "DECAY", decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "SUSTAIN", sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "RELEASE", releaseSlider);

    updateOscTabs();
    startTimerHz (30);
    setSize (1000, 600);
    addMouseListener (this, true);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::updateOscTabs()
{
    for (int i = 0; i < 7; ++i)
    {
        bool show = oscTabButtons[i].getToggleState();

        waveformDisplays[i].setVisible (show);
        waveformSelectors[i].setVisible (show);
        oscMixSliders[i].setVisible (show);
        oscPanSliders[i].setVisible (show);

        if (i > 0)
            oscDetuneSliders[i].setVisible (show);
    }
}

void PluginEditor::visibilityChanged()
{
    if (isShowing())
        keyboardComponent.grabKeyboardFocus();
}

void PluginEditor::mouseDown (const juce::MouseEvent& e)
{
    if (dynamic_cast<juce::TextEditor*> (e.originalComponent) == nullptr)
        keyboardComponent.grabKeyboardFocus();
}

void PluginEditor::timerCallback()
{
    auto readScopeBuffer = [this] (AudioBufferFifo<float>& fifo, int numChannels) -> bool {
        auto numReady = fifo.getNumReady();
        if (numReady <= 0)
            return false;

        tempScopeBuffer.setSize (numChannels, numReady);
        fifo.pop (tempScopeBuffer);

        constexpr auto scopeDisplayGain = 4.0f;
        tempScopeBuffer.applyGain (scopeDisplayGain);

        for (auto channel = 0; channel < tempScopeBuffer.getNumChannels(); ++channel)
            juce::FloatVectorOperations::clip (tempScopeBuffer.getWritePointer (channel),
                tempScopeBuffer.getReadPointer (channel),
                -1.0f,
                1.0f,
                tempScopeBuffer.getNumSamples());

        return true;
    };

    if (readScopeBuffer (processorRef.scopeFifo, processorRef.getTotalNumOutputChannels()))
    {
        outputVisualiser.setSampleRate (processorRef.getSampleRate());
        outputVisualiser.pushBuffer (tempScopeBuffer);
    }

    for (int i = 0; i < 7; ++i)
        if (readScopeBuffer (*processorRef.oscScopeFifos[i], 2))
            waveformDisplays[i].pushBuffer (tempScopeBuffer);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    keyboardComponent.setBounds (area.removeFromBottom (100));

    auto bottomSection = area.removeFromBottom (240);
    outputVisualiser.setBounds (area.removeFromTop (128));

    auto partWidth = bottomSection.getWidth() / 2;

    // --- OSC section (left half) ---
    auto oscArea = bottomSection.removeFromLeft (partWidth).reduced (10);
    oscGroup.setBounds (oscArea);

    // Tab row – 7 equal buttons across full width
    auto tabsArea = juce::Rectangle<int> (oscArea.getX() + 15, oscArea.getY(), oscArea.getWidth() - 15, 24);
    auto tabWidth = tabsArea.getWidth() / 7;
    for (int i = 0; i < 7; ++i)
    {
        if (i < 6)
            oscTabButtons[i].setBounds (tabsArea.removeFromLeft (tabWidth));
        else
            oscTabButtons[i].setBounds (tabsArea); // last tab takes remaining space
    }

    auto oscContent = oscArea.withTop (oscArea.getY() + 24).reduced (10);

    // Detune + Pan knobs at bottom
    auto detuneBounds = oscContent.removeFromBottom (90);
    auto detuneSliderBounds = detuneBounds.withSizeKeepingCentre (140, 70).withTrimmedTop (15);
    auto detuneLeft = detuneSliderBounds.removeFromLeft (70);
    auto detuneRight = detuneSliderBounds;

    // OSC 1: only Pan
    oscPanSliders[0].setBounds (detuneRight);

    // OSC 2-7: Detune + Pan
    for (int i = 1; i < 7; ++i)
    {
        oscDetuneSliders[i].setBounds (detuneLeft);
        oscPanSliders[i].setBounds (detuneRight);
    }

    // Wave selector + mix slider (right strip)
    auto oscControls = oscContent.removeFromRight (100);

    auto selectorBounds = oscControls.removeFromTop (24);
    for (auto& sel : waveformSelectors)
        sel.setBounds (selectorBounds);

    auto mixBounds = oscControls.removeFromTop (24).withTrimmedTop (4);
    for (auto& mix : oscMixSliders)
        mix.setBounds (mixBounds);

    // Waveform display fills remaining space
    auto displayBounds = oscContent.withTrimmedRight (10);
    for (auto& display : waveformDisplays)
        display.setBounds (displayBounds);

    // --- Right section: Filter + Amp Env stacked ---
    auto rightArea = bottomSection;

    auto filterArea = rightArea.removeFromTop (rightArea.getHeight() / 2).reduced (10);
    filterGroup.setBounds (filterArea);
    auto filterContent = filterArea.withTop (filterArea.getY() + 30).reduced (10);
    cutoffSlider.setBounds (filterContent.removeFromLeft (filterContent.getWidth() / 2));
    resonanceSlider.setBounds (filterContent);

    auto envArea = rightArea.reduced (10);
    ampEnvGroup.setBounds (envArea);
    auto envContent = envArea.withTop (envArea.getY() + 30).reduced (10);
    auto envKnobWidth = envContent.getWidth() / 4;
    attackSlider.setBounds (envContent.removeFromLeft (envKnobWidth));
    decaySlider.setBounds (envContent.removeFromLeft (envKnobWidth));
    sustainSlider.setBounds (envContent.removeFromLeft (envKnobWidth));
    releaseSlider.setBounds (envContent);
}
