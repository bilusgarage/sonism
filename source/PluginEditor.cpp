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

        int connectedEdges = 0;
        if (i > 0)
            connectedEdges |= juce::Button::ConnectedOnLeft;
        if (i < 6)
            connectedEdges |= juce::Button::ConnectedOnRight;
        btn.setConnectedEdges (connectedEdges);

        addAndMakeVisible (btn);
    }

    // Set up 7 oscillator controls
    juce::StringArray waveItems { "Sine", "Triangle", "Square", "Sawtooth", "Pulse" };
    for (int i = 0; i < 7; ++i)
    {
        juce::String idStr = juce::String (i + 1);

        // Waveform display
        if (i >= 1 && i <= 3) waveformDisplays[i].setColour (juce::Colours::purple);
        else if (i >= 4 && i <= 6) waveformDisplays[i].setColour (juce::Colours::green);
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

    // Unison Detune knob (global)
    unisonDetuneSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    unisonDetuneSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible (unisonDetuneSlider);

    unisonDetuneLabel.setText ("Unison", juce::dontSendNotification);
    unisonDetuneLabel.attachToComponent (&unisonDetuneSlider, false);
    unisonDetuneLabel.setJustificationType (juce::Justification::centred);

    unisonDetuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "UNISONDETUNE", unisonDetuneSlider);

    unisonDetuneSlider.onValueChange = [this] {
        auto amount = (float) unisonDetuneSlider.getValue();
        for (int i = 1; i < 7; ++i)
        {
            // Oscillator pairs: 1-2, 3-4, 5-6 (Osc 2-3, 4-5, 6-7)
            int pairIdx = (i - 1) / 2; // 0, 1, 2
            float mag = (float)(pairIdx + 1) / 3.0f; // 0.33, 0.66, 1.0
            float sign = (i % 2 != 0) ? -1.0f : 1.0f; // left/down for odd i, right/up for even i
            
            float value = amount * mag * sign;

            juce::String paramId = "OSC" + juce::String (i + 1) + "DETUNE";
            if (auto* param = processorRef.apvts.getParameter (paramId))
                param->setValueNotifyingHost (param->convertTo0to1 (value));
        }
    };

    // Unison Spread knob (global)
    unisonSpreadSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    unisonSpreadSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible (unisonSpreadSlider);

    unisonSpreadLabel.setText ("Spread", juce::dontSendNotification);
    unisonSpreadLabel.attachToComponent (&unisonSpreadSlider, false);
    unisonSpreadLabel.setJustificationType (juce::Justification::centred);

    unisonSpreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "UNISONSPREAD", unisonSpreadSlider);

    unisonSpreadSlider.onValueChange = [this] {
        auto amount = (float) unisonSpreadSlider.getValue();
        for (int i = 1; i < 7; ++i)
        {
            // Oscillator pairs: 1-2, 3-4, 5-6 (Osc 2-3, 4-5, 6-7)
            int pairIdx = (i - 1) / 2; // 0, 1, 2
            float mag = (float)(pairIdx + 1) / 3.0f; // 0.33, 0.66, 1.0
            float sign = (i % 2 != 0) ? -1.0f : 1.0f; // left for odd i, right for even i
            
            float value = amount * mag * sign;

            juce::String paramId = "OSC" + juce::String (i + 1) + "SPREAD";
            if (auto* param = processorRef.apvts.getParameter (paramId))
                param->setValueNotifyingHost (param->convertTo0to1 (value));
        }
    };

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

    // --- LFO Section Initialization ---
    addAndMakeVisible (lfoGroup);
    addAndMakeVisible (lfoTab1);
    addAndMakeVisible (lfoTab2);
    lfoTab1.setRadioGroupId (2);
    lfoTab2.setRadioGroupId (2);
    lfoTab1.setClickingTogglesState (true);
    lfoTab2.setClickingTogglesState (true);
    lfoTab1.setToggleState (true, juce::dontSendNotification);
    lfoTab1.setConnectedEdges (juce::Button::ConnectedOnRight);
    lfoTab2.setConnectedEdges (juce::Button::ConnectedOnLeft);

    auto setupWaveButton = [this] (juce::ShapeButton& btn, juce::Path path, int lfoIdx, int waveIdx) {
        btn.setShape (path, true, true, false);
        juce::Colour c = (lfoIdx == 0) ? juce::Colours::purple : juce::Colours::green;
        btn.setOnColours (c, c, c);
        btn.setRadioGroupId (3 + lfoIdx);
        btn.setClickingTogglesState (true);
        addAndMakeVisible (btn);
        btn.onClick = [this, lfoIdx, waveIdx] {
            if (auto* param = processorRef.apvts.getParameter ("LFO" + juce::String (lfoIdx + 1) + "WAVETYPE"))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) waveIdx));
            int displayWave = (waveIdx == 0) ? 0 : ((waveIdx == 3) ? 1 : 2); // 0=Sine, 1=Saw, 2=Tri for display
            lfoDisplay.setParameters (displayWave, lfoPhaseSlider[lfoIdx].getValue(), lfoAmountSlider[lfoIdx].getValue());
        };
    };

    juce::Path sinePath;
    for (int i = 0; i <= 100; ++i)
    {
        float x = (float) i / 100.0f;
        float y = std::sin (x * juce::MathConstants<float>::twoPi) * -0.5f + 0.5f;
        if (i == 0)
            sinePath.startNewSubPath (x, y);
        else
            sinePath.lineTo (x, y);
    }

    juce::Path sawPath;
    sawPath.startNewSubPath (0.0f, 1.0f);
    sawPath.lineTo (1.0f, 0.0f);
    sawPath.lineTo (1.0f, 1.0f);

    juce::Path triPath;
    triPath.startNewSubPath (0.0f, 0.5f);
    triPath.lineTo (0.25f, 0.0f);
    triPath.lineTo (0.75f, 1.0f);
    triPath.lineTo (1.0f, 0.5f);

    auto setupLfoKnob = [this] (juce::Slider& s, juce::Label& l, juce::String text) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 16);
        addAndMakeVisible (s);
        l.setText (text, juce::dontSendNotification);
        l.attachToComponent (&s, false);
        l.setJustificationType (juce::Justification::centred);
    };

    for (int i = 0; i < 2; ++i)
    {
        setupWaveButton (lfoSineButton[i], sinePath, i, 0); // Sine is index 0
        setupWaveButton (lfoSawButton[i], sawPath, i, 3); // Saw is index 3
        setupWaveButton (lfoTriangleButton[i], triPath, i, 1); // Tri is index 1

        lfoSineButton[i].setToggleState (true, juce::dontSendNotification);

        addAndMakeVisible (lfoSyncButton[i]);
        lfoSyncButton[i].setClickingTogglesState (true);

        setupLfoKnob (lfoAmountSlider[i], lfoAmountLabel[i], "Amount");
        setupLfoKnob (lfoRateSlider[i], lfoRateLabel[i], "Rate");
        setupLfoKnob (lfoPhaseSlider[i], lfoPhaseLabel[i], "Phase");

        juce::String idStr = juce::String (i + 1);
        lfoAmountAttachment[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "LFO" + idStr + "AMOUNT", lfoAmountSlider[i]);
        lfoRateAttachment[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "LFO" + idStr + "RATE", lfoRateSlider[i]);
        lfoPhaseAttachment[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "LFO" + idStr + "PHASE", lfoPhaseSlider[i]);

        lfoPhaseSlider[i].onValueChange = [this, i] {
            int wave = lfoSineButton[i].getToggleState() ? 0 : (lfoSawButton[i].getToggleState() ? 1 : 2);
            lfoDisplay.setParameters (wave, lfoPhaseSlider[i].getValue(), lfoAmountSlider[i].getValue());
        };
        lfoAmountSlider[i].onValueChange = lfoPhaseSlider[i].onValueChange;
    }

    addAndMakeVisible (lfoDisplay);
    lfoDisplay.setParameters (0, 0.0f, 1.0f);

    lfoTab1.onClick = [this] { updateLfoTabs(); };
    lfoTab2.onClick = [this] { updateLfoTabs(); };

    // --- Effects Section Initialization ---
    addAndMakeVisible (effectsGroup);
    addAndMakeVisible (reverbTile);
    addAndMakeVisible (delayTile);
    addAndMakeVisible (compressorTile);

    // Preset Header section
    pluginNameLabel.setText ("Sonism", juce::dontSendNotification);
    pluginNameLabel.setFont (juce::Font (24.0f, juce::Font::bold));
    pluginNameLabel.setJustificationType (juce::Justification::centredLeft);
    pluginNameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (pluginNameLabel);

    addAndMakeVisible (presetButton);
    addAndMakeVisible (presetLeftButton);
    addAndMakeVisible (presetRightButton);

    updateOscTabs();
    updateLfoTabs();
    startTimerHz (30);
    setSize (1000, 800);
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

void PluginEditor::updateLfoTabs()
{
    int activeIdx = lfoTab1.getToggleState() ? 0 : 1;
    for (int i = 0; i < 2; ++i)
    {
        bool isVisible = (i == activeIdx);
        lfoSineButton[i].setVisible (isVisible);
        lfoSawButton[i].setVisible (isVisible);
        lfoTriangleButton[i].setVisible (isVisible);
        lfoSyncButton[i].setVisible (isVisible);
        lfoAmountSlider[i].setVisible (isVisible);
        lfoRateSlider[i].setVisible (isVisible);
        lfoPhaseSlider[i].setVisible (isVisible);
    }

    // Update display to match the active tab
    int wave = lfoSineButton[activeIdx].getToggleState() ? 0 : (lfoSawButton[activeIdx].getToggleState() ? 1 : 2);
    lfoDisplay.setParameters (wave, lfoPhaseSlider[activeIdx].getValue(), lfoAmountSlider[activeIdx].getValue());
    lfoDisplay.setColour (activeIdx == 0 ? juce::Colours::purple : juce::Colours::green);
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
    
    // Header for presets
    auto headerArea = area.removeFromTop (50).reduced(10, 10);
    pluginNameLabel.setBounds (headerArea.removeFromLeft (200));
    
    auto presetArea = juce::Rectangle<int> ((getWidth() - 360) / 2, headerArea.getY(), 360, 30);
    presetLeftButton.setBounds (presetArea.removeFromLeft (30));
    presetRightButton.setBounds (presetArea.removeFromRight (30));
    presetButton.setBounds (presetArea.reduced (5, 0));

    keyboardComponent.setBounds (area.removeFromBottom (100));

    auto lfoEffectsArea = area.removeFromBottom (257); // 232 + 25
    auto bottomSection = area.removeFromBottom (265); // 240 + 25
    outputVisualiser.setBounds (area.removeFromTop (128)); // 800 - 50 - 100 - 257 - 265 = 128

    auto partWidth = bottomSection.getWidth() / 2;

    // --- OSC section (left half) ---
    auto oscArea = bottomSection.removeFromLeft (partWidth).reduced (10);
    oscGroup.setBounds (oscArea);

    // Tab row – 7 buttons centered, smaller size
    int tabWidth = 65;
    int totalTabsWidth = tabWidth * 7;
    auto tabsArea = juce::Rectangle<int> (oscArea.getCentreX() - totalTabsWidth / 2, oscArea.getY(), totalTabsWidth, 22);
    for (int i = 0; i < 7; ++i)
    {
        oscTabButtons[i].setBounds (tabsArea.removeFromLeft (tabWidth));
    }

    auto oscContent = oscArea.withTop (oscArea.getY() + 18).reduced (10);

    // Bottom controls row: wave selector + mix on left, detune + pan + unison knobs on right
    auto controlsRow = oscContent.removeFromBottom (90);

    // Right side of controls row: Detune + Pan + Unison + Spread knobs
    auto knobsArea = controlsRow.removeFromRight (280);
    auto knobsBounds = knobsArea.withSizeKeepingCentre (280, 70).withTrimmedTop (15);
    auto detuneLeft = knobsBounds.removeFromLeft (70);
    auto panMiddle = knobsBounds.removeFromLeft (70);
    auto unisonArea = knobsBounds.removeFromLeft (70);
    auto spreadArea = knobsBounds;

    // Global knobs are always visible
    unisonDetuneSlider.setBounds (unisonArea);
    unisonSpreadSlider.setBounds (spreadArea);

    // OSC 1: only Pan
    oscPanSliders[0].setBounds (panMiddle);

    // OSC 2-7: Detune + Pan
    for (int i = 1; i < 7; ++i)
    {
        oscDetuneSliders[i].setBounds (detuneLeft);
        oscPanSliders[i].setBounds (panMiddle);
    }

    // Left side of controls row: Wave selector + Mix slider
    auto leftControls = controlsRow.withTrimmedTop (15);

    auto selectorBounds = leftControls.removeFromTop (24);
    for (auto& sel : waveformSelectors)
        sel.setBounds (selectorBounds);

    auto mixBounds = leftControls.removeFromTop (24).withTrimmedTop (4);
    for (auto& mix : oscMixSliders)
        mix.setBounds (mixBounds);

    // Waveform display fills remaining space above the controls
    auto displayBounds = oscContent;
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

    // --- LFO Section (below OSC) ---
    auto lfoArea = lfoEffectsArea.removeFromLeft (partWidth).reduced (10);
    lfoGroup.setBounds (lfoArea);

    int lfoTabW = 65;
    auto lfoTabsBounds = juce::Rectangle<int> (lfoArea.getX() + 10, lfoArea.getY(), lfoTabW * 2, 22);
    lfoTab1.setBounds (lfoTabsBounds.removeFromLeft (lfoTabW));
    lfoTab2.setBounds (lfoTabsBounds);

    auto lfoContent = lfoArea.withTop (lfoArea.getY() + 18).reduced (10);
    auto lfoControlsRow = lfoContent.removeFromTop (60);

    auto lfoWavesArea = lfoControlsRow.removeFromLeft (120);
    int waveBtnW = 40;

    auto syncBtnArea = lfoControlsRow.removeFromLeft (40);
    auto lfoKnobsArea = lfoControlsRow;
    int lfoKnobW = lfoKnobsArea.getWidth() / 3;

    for (int i = 0; i < 2; ++i)
    {
        lfoSineButton[i].setBounds (lfoWavesArea.withWidth (waveBtnW).reduced (2));
        lfoSawButton[i].setBounds (lfoWavesArea.withX (lfoWavesArea.getX() + waveBtnW).withWidth (waveBtnW).reduced (2));
        lfoTriangleButton[i].setBounds (lfoWavesArea.withX (lfoWavesArea.getX() + waveBtnW * 2).withWidth (waveBtnW).reduced (2));

        lfoSyncButton[i].setBounds (syncBtnArea.reduced (2, 15));

        lfoAmountSlider[i].setBounds (lfoKnobsArea.withWidth (lfoKnobW));
        lfoRateSlider[i].setBounds (lfoKnobsArea.withX (lfoKnobsArea.getX() + lfoKnobW).withWidth (lfoKnobW));
        lfoPhaseSlider[i].setBounds (lfoKnobsArea.withX (lfoKnobsArea.getX() + lfoKnobW * 2).withWidth (lfoKnobW));
    }

    lfoDisplay.setBounds (lfoContent.withTrimmedTop (10));

    // --- Effects Section (below ENV) ---
    auto effectsArea = lfoEffectsArea.reduced (10);
    effectsGroup.setBounds (effectsArea);

    auto effectsContent = effectsArea.withTop (effectsArea.getY() + 18).reduced (10);
    int tileHeight = effectsContent.getHeight() / 3;
    reverbTile.setBounds (effectsContent.removeFromTop (tileHeight).reduced (0, 5));
    delayTile.setBounds (effectsContent.removeFromTop (tileHeight).reduced (0, 5));
    compressorTile.setBounds (effectsContent.reduced (0, 5));
}
