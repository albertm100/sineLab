/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void applyPulseWaveToKey (SineLabAudioProcessor&, int, double, double = 0.0);
static bool isPrime (int n);


static void drawSmoothedPath (juce::Graphics& g, const juce::Array<juce::Point<float>>& pts, float thickness = 2.0f)
{
    if (pts.size() < 2) return;
    juce::Path path;
    path.startNewSubPath (pts[0]);
    for (int i = 0; i < pts.size() - 1; ++i)
    {
        auto p0 = pts[juce::jmax (0, i - 1)];
        auto p1 = pts[i];
        auto p2 = pts[i + 1];
        auto p3 = pts[juce::jmin (pts.size() - 1, i + 2)];
        path.cubicTo ({ p1.x + (p2.x - p0.x) / 6.0f, p1.y + (p2.y - p0.y) / 6.0f },
                      { p2.x - (p3.x - p1.x) / 6.0f, p2.y - (p3.y - p1.y) / 6.0f }, p2);
    }
    g.strokePath (path, juce::PathStrokeType (thickness));
}

//==============================================================================
SineLabAudioProcessorEditor::SineLabAudioProcessorEditor (SineLabAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setResizable (true, true);
    setResizeLimits (396, 250, 2376, 1500);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (792.0 / 500.0);
    setSize (792, 500);
    setWantsKeyboardFocus (true);

    for (auto* child : getChildren())
    {
        if (auto* resizer = dynamic_cast<juce::ResizableCornerComponent*> (child))
        {
            resizer->addMouseListener (this, false);
            break;
        }
    }

    getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId,            juce::Colours::white);
    getLookAndFeel().setColour (juce::PopupMenu::textColourId,                  juce::Colours::black);
    getLookAndFeel().setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::black);
    getLookAndFeel().setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    getLookAndFeel().setColour (juce::AlertWindow::backgroundColourId,          juce::Colours::white);
    getLookAndFeel().setColour (juce::AlertWindow::textColourId,                juce::Colours::black);
    getLookAndFeel().setColour (juce::AlertWindow::outlineColourId,             juce::Colours::black);
    getLookAndFeel().setColour (juce::TextButton::buttonColourId,               juce::Colours::white);
    getLookAndFeel().setColour (juce::TextButton::textColourOffId,              juce::Colours::black);

    audioProcessor.addChangeListener (this);
    audioProcessor.updateActiveRanks();

    for (int i = 0; i < 88; ++i)
    {
        auto* button = new InvisibleKeyButton();

        int noteIndex = i;
        button->onClick = [this, noteIndex] { keyClicked (noteIndex); };
        button->onHoverChange = [this, noteIndex] (bool isOver)
        {
            hoveredKey = isOver ? noteIndex : -1;
            repaintKeyboard();
        };

        addAndMakeVisible (button);
        keyButtons.add (button);
    }

    addAndMakeVisible (toggleTabButton);
    addAndMakeVisible (ampTabButton);
    addAndMakeVisible (tuningTabButton);
    addAndMakeVisible (phaseTabButton);
    addAndMakeVisible (panTabButton);
    addAndMakeVisible (attackTabButton);
    addAndMakeVisible (decayTabButton);
    addAndMakeVisible (sustainTabButton);
    addAndMakeVisible (releaseTabButton);

    toggleTabButton.onClick = [this] { tabSelected (0); };
    ampTabButton.onClick = [this] { tabSelected (1); };
    tuningTabButton.onClick = [this] { tabSelected (2); };
    phaseTabButton.onClick = [this] { tabSelected (3); };
    panTabButton.onClick = [this] { tabSelected (4); };
    attackTabButton.onClick = [this] { tabSelected (5); };
    decayTabButton.onClick = [this] { tabSelected (6); };
    sustainTabButton.onClick = [this] { tabSelected (7); };
    releaseTabButton.onClick = [this] { tabSelected (8); };

    addAndMakeVisible (toggleViewport);
    toggleViewport.setViewedComponent (&toggleContainer, false);
    toggleViewport.setScrollBarsShown (true, false);
    toggleViewport.setVisible (false);

    toggleGraphViewport.setViewedComponent (&toggleGraphComponent, false);
    toggleGraphViewport.setScrollBarsShown (true, false);
    toggleGraphViewport.setVisible (false);
    addAndMakeVisible (toggleGraphViewport);
    addAndMakeVisible (globalToggleCheckbox);
    globalToggleCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    globalToggleCheckbox.setButtonText ("");
    globalToggleCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    globalToggleCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    globalToggleCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    globalToggleCheckbox.onClick = [this] { globalToggleChanged(); };
    globalToggleCheckbox.setVisible (false);
    
    
    addAndMakeVisible (firstHarmonicToggleCheckbox);
    firstHarmonicToggleCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    firstHarmonicToggleCheckbox.setButtonText ("");
    firstHarmonicToggleCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    firstHarmonicToggleCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    firstHarmonicToggleCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    firstHarmonicToggleCheckbox.setToggleState (true, juce::dontSendNotification);
    firstHarmonicToggleCheckbox.onClick = [this] { firstHarmonicToggleChanged(); };
    firstHarmonicToggleCheckbox.setVisible (false);

    addAndMakeVisible (firstHarmonicLabel);
    firstHarmonicLabel.setText ("1st Harm", juce::dontSendNotification);
    firstHarmonicLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    firstHarmonicLabel.setJustificationType (juce::Justification::centredRight);
    firstHarmonicLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    firstHarmonicLabel.setVisible (false);
    
    addAndMakeVisible (evensToggleCheckbox);
    evensToggleCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    evensToggleCheckbox.setButtonText ("");
    evensToggleCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    evensToggleCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    evensToggleCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    evensToggleCheckbox.setToggleState (true, juce::dontSendNotification);
    evensToggleCheckbox.onClick = [this] { evensToggleChanged(); };
    evensToggleCheckbox.setVisible (false);

    addAndMakeVisible (evensLabel);
    evensLabel.setText ("Evens", juce::dontSendNotification);
    evensLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    evensLabel.setJustificationType (juce::Justification::centredRight);
    evensLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    evensLabel.setVisible (false);

    addAndMakeVisible (primesToggleCheckbox);
    primesToggleCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    primesToggleCheckbox.setButtonText ("");
    primesToggleCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    primesToggleCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    primesToggleCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    primesToggleCheckbox.setToggleState (true, juce::dontSendNotification);
    primesToggleCheckbox.onClick = [this] { primesToggleChanged(); };
    primesToggleCheckbox.setVisible (false);

    addAndMakeVisible (primesLabel);
    primesLabel.setText ("Primes", juce::dontSendNotification);
    primesLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    primesLabel.setJustificationType (juce::Justification::centredRight);
    primesLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    primesLabel.setVisible (false);

    addAndMakeVisible (everyNToggleCheckbox);
    everyNToggleCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    everyNToggleCheckbox.setButtonText ("");
    everyNToggleCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    everyNToggleCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    everyNToggleCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    everyNToggleCheckbox.setToggleState (true, juce::dontSendNotification);
    everyNToggleCheckbox.onClick = [this] { everyNToggleChanged(); };
    everyNToggleCheckbox.setVisible (false);

    setupGlobalTextBox (everyNTextBox, "0123456789", 2,
        [this]
        {
            int newVal = juce::jlimit (3, 99, everyNTextBox.getText().getIntValue());
            everyNTextBox.setText (juce::String (newVal), juce::dontSendNotification);

            bool isOff = ! everyNToggleCheckbox.getToggleState();
            if (isOff)
            {
                int oldN = audioProcessor.everyNValue;
                for (int key = 0; key < 88; ++key)
                {
                    int s = audioProcessor.keyStartIndex[key];
                    int c = audioProcessor.harmonicCounts[key];
                    for (int h = oldN - 1; h < c; h += oldN)
                        audioProcessor.oscillators[s + h].manuallyMuted = false;
                    for (int h = newVal - 1; h < c; h += newVal)
                        audioProcessor.oscillators[s + h].manuallyMuted = true;
                }
                audioProcessor.updateActiveRanks();
                recalculateAllKeyVolumes();
                repaint();
                if (selectedKey != -1 && activeTab == 0)
                    rebuildToggleButtons();
            }

            audioProcessor.everyNValue = newVal;
        },
        nullptr);

    ampSubTabSliderLookAndFeel.trackThickness   = 1.0f;
    ampSubTabSliderLookAndFeel.fixedThumbRadius = true;

    addAndMakeVisible (ampSubTabSlider);
    ampSubTabSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    ampSubTabSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    ampSubTabSlider.setRange (1.0, 3.0, 1.0);
    ampSubTabSlider.setValue (1.0, juce::dontSendNotification);
    ampSubTabSlider.setColour (juce::Slider::thumbColourId, juce::Colours::black);
    ampSubTabSlider.setLookAndFeel (&ampSubTabSliderLookAndFeel);
    ampSubTabSlider.setVisible (false);
    ampSubTabSlider.onValueChange = [this]
    {
        int newTab = (int) ampSubTabSlider.getValue();
        if (newTab == activeAmpSubTab) return;
        activeAmpSubTab = newTab;
        int osc0_  = audioProcessor.keyStartIndex[0];
        int osc87_ = audioProcessor.keyStartIndex[87];
        if (activeAmpSubTab == 1)
        {
            ampA0TextBox.setText (juce::String (audioProcessor.lastAppliedAmpA0,  4), juce::dontSendNotification);
            ampC8TextBox.setText (juce::String (audioProcessor.lastAppliedAmpC8, 4), juce::dontSendNotification);
            ampA0UpperTextBox.setText (juce::String (audioProcessor.lastAppliedAmpStartKey + 1), juce::dontSendNotification);
            ampC8UpperTextBox.setText (juce::String (audioProcessor.lastAppliedAmpEndKey + 1), juce::dontSendNotification);
            globalAmpTextBox.setText (juce::String (audioProcessor.globalAmpValue, 4), juce::dontSendNotification);
            expSteepnessTextBox.setText (juce::String (audioProcessor.lastAppliedExpKAmp), juce::dontSendNotification);
        }
        else if (activeAmpSubTab == 2)
        {
            ampA0TextBox.setText (juce::String (audioProcessor.keyVolume[0],  4), juce::dontSendNotification);
            ampC8TextBox.setText (juce::String (audioProcessor.keyVolume[87], 4), juce::dontSendNotification);
            ampA0UpperTextBox.setText (juce::String (audioProcessor.lastAppliedKeyVolumeStartKey + 1), juce::dontSendNotification);
            ampC8UpperTextBox.setText (juce::String (audioProcessor.lastAppliedKeyVolumeEndKey + 1), juce::dontSendNotification);
            globalAmpTextBox.setText (juce::String (audioProcessor.globalAmpValue, 4), juce::dontSendNotification);
            expSteepnessTextBox.setText (juce::String (audioProcessor.lastAppliedExpKKeyVolume), juce::dontSendNotification);
        }
        else if (activeAmpSubTab == 3)
        {
            expSteepnessTextBox.setText      (juce::String (audioProcessor.lastAppliedExpKEvenMorph), juce::dontSendNotification);
            evenMorphA0UpperTextBox.setText  (juce::String (audioProcessor.lastAppliedEvenMorphStartKey + 1), juce::dontSendNotification);
            evenMorphC8UpperTextBox.setText  (juce::String (audioProcessor.lastAppliedEvenMorphEndKey   + 1), juce::dontSendNotification);
            evenMorphA0CenterTextBox.setText (juce::String (audioProcessor.lastAppliedNToNSquaredStartKey + 1), juce::dontSendNotification);
            evenMorphC8CenterTextBox.setText (juce::String (audioProcessor.lastAppliedNToNSquaredEndKey   + 1), juce::dontSendNotification);
            ampA0TextBox.setText (juce::String (audioProcessor.evenMorphStrength[0],  4), juce::dontSendNotification);
            ampC8TextBox.setText (juce::String (audioProcessor.evenMorphStrength[87], 4), juce::dontSendNotification);
        }
        wireShapeButtons();
        updateControlVisibility();
        repaint();
    };


    addAndMakeVisible (amplitudeViewport);
    amplitudeViewport.setViewedComponent (&amplitudeContainer, false);
    amplitudeViewport.setScrollBarsShown (false, true);
    amplitudeViewport.setVisible (false);
    setupSlider (keyVolumeSlider, juce::Slider::LinearVertical, 0.0, 1.0, 0.0001, [this] { keyVolumeChanged(); });

    addAndMakeVisible (normalizationCheckbox);
    normalizationCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    normalizationCheckbox.setButtonText ("");
    normalizationCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    normalizationCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    normalizationCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    normalizationCheckbox.onClick = [this] { normalizationToggled(); };
    normalizationCheckbox.setVisible (false);

    addAndMakeVisible (taperCTCheckbox);
    taperCTCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    taperCTCheckbox.setButtonText ("TAPER CT");
    taperCTCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    taperCTCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    taperCTCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    taperCTCheckbox.onClick = [this] { taperCTToggled(); };
    taperCTCheckbox.setVisible (false);

    addAndMakeVisible (taperACTCheckbox);
    taperACTCheckbox.setLookAndFeel (&scalingToggleLookAndFeel);
    taperACTCheckbox.setButtonText ("TAPER ACT");
    taperACTCheckbox.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    taperACTCheckbox.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    taperACTCheckbox.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    taperACTCheckbox.onClick = [this] { taperACTToggled(); };
    taperACTCheckbox.setVisible (false);




    addAndMakeVisible (applyNToXButton);
    applyNToXButton.setButtonText ("n^x");
    applyNToXButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    applyNToXButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    applyNToXButton.onClick = [this] { applyNToXClicked(); };
    applyNToXButton.setVisible (false);

    addAndMakeVisible (nToXTextBox);
    nToXTextBox.setJustification (juce::Justification::centred);
    nToXTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    nToXTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    nToXTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    nToXTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    nToXTextBox.setInputRestrictions (4, "0123456789.");
    nToXTextBox.setText ("1.00", juce::dontSendNotification);
    nToXTextBox.onReturnKey = [this]
    {
        applyNToXClicked();
        unfocusAllComponents();
    };
    nToXTextBox.onFocusLost = [this]
    {
        applyNToXClicked();
    };
    nToXTextBox.setVisible (false);

    addAndMakeVisible (integralTextBox);
    integralTextBox.setJustification (juce::Justification::centred);
    integralTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    integralTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    integralTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    integralTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    integralTextBox.setInputRestrictions (4, "0123456789.");
    integralTextBox.setText ("0.00", juce::dontSendNotification);
    integralTextBox.onReturnKey = [this]
    {
        integralApplied();
        unfocusAllComponents();
    };
    integralTextBox.onFocusLost = [this] { integralApplied(); };
    integralTextBox.setVisible (false);

    addAndMakeVisible (integralLabel);
    integralLabel.setText ("INTEGRAL", juce::dontSendNotification);
    integralLabel.setJustificationType (juce::Justification::centredLeft);
    integralLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    integralLabel.setVisible (false);


    addAndMakeVisible (evenLeftOddRightButton);
    evenLeftOddRightButton.setButtonText ("ELOR");
    evenLeftOddRightButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    evenLeftOddRightButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    evenLeftOddRightButton.onClick = [this] { evenLeftOddRightClicked(); };
    evenLeftOddRightButton.setVisible (false);
    
    addAndMakeVisible (oddLeftEvenRightButton);
    oddLeftEvenRightButton.setButtonText ("OLER");
    oddLeftEvenRightButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    oddLeftEvenRightButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    oddLeftEvenRightButton.onClick = [this] { oddLeftEvenRightClicked(); };
    oddLeftEvenRightButton.setVisible (false);

    addAndMakeVisible (twoLeftButton);
    twoLeftButton.setButtonText ("2L");
    twoLeftButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    twoLeftButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    twoLeftButton.onClick = [this] { twoLeftClicked(); };
    twoLeftButton.setVisible (false);

    addAndMakeVisible (alternateActiveButton);
        alternateActiveButton.setButtonText ("LR");
        alternateActiveButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
        alternateActiveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        alternateActiveButton.onClick = [this] { alternateActiveClicked(); };
        alternateActiveButton.setVisible (false);

    addAndMakeVisible (smartPanButton);
    smartPanButton.setButtonText ("SMART");
    smartPanButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    smartPanButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    smartPanButton.onClick = [this] { smartPanClicked(); };
    smartPanButton.setVisible (false);

    setupGlobalTextBox (toggleA0TextBox, "0123456789", 3,
        [this]
        {
            int ceiling = juce::jlimit (1, audioProcessor.harmonicCounts[0], toggleA0TextBox.getText().getIntValue());
            toggleA0TextBox.setText (juce::String (ceiling), juce::dontSendNotification);
            audioProcessor.lastAppliedToggleA0 = ceiling;
            int base = audioProcessor.keyStartIndex[0];
            for (int i = 0; i < audioProcessor.harmonicCounts[0]; ++i)
                audioProcessor.oscillators[base + i].aboveCeiling = (i >= ceiling);
            recalculateKeyVolume (0);
            audioProcessor.updateActiveRanks();
            if (selectedKey == 0 && activeTab == 0) rebuildToggleButtons();
            toggleGraphComponent.repaint();
            repaintGraphArea();
        },
        [] (float) {});
    toggleA0TextBox.setText (juce::String (audioProcessor.lastAppliedToggleA0), juce::dontSendNotification);
    setupGlobalTextBox (toggleC8TextBox, "0123456789", 1,
        [this]
        {
            int ceiling = juce::jlimit (1, audioProcessor.harmonicCounts[87], toggleC8TextBox.getText().getIntValue());
            toggleC8TextBox.setText (juce::String (ceiling), juce::dontSendNotification);
            audioProcessor.lastAppliedToggleC8 = ceiling;
            int base = audioProcessor.keyStartIndex[87];
            for (int i = 0; i < audioProcessor.harmonicCounts[87]; ++i)
                audioProcessor.oscillators[base + i].aboveCeiling = (i >= ceiling);
            recalculateKeyVolume (87);
            audioProcessor.updateActiveRanks();
            if (selectedKey == 87 && activeTab == 0) rebuildToggleButtons();
            toggleGraphComponent.repaint();
            repaintGraphArea();
        },
        [] (float) {});
    toggleC8TextBox.setText (juce::String (audioProcessor.lastAppliedToggleC8), juce::dontSendNotification);

    setupGlobalTextBox (globalAmpTextBox, "0123456789.", 6,
            [this] { globalAmpApplied(); },
            [this] (float delta)
            {
                double value = juce::jlimit (0.0, 1.0, audioProcessor.globalAmpValue + delta * 0.01);
                globalAmpTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
                audioProcessor.globalAmpValue = value;
                for (auto& osc : audioProcessor.oscillators)
                    osc.amplitude = value;
                recalculateAllKeyVolumes();
            });

    addAndMakeVisible (presetBrowserComboBox);
    presetBrowserComboBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
    presetBrowserComboBox.setColour (juce::ComboBox::textColourId,       juce::Colours::black);
    presetBrowserComboBox.setColour (juce::ComboBox::arrowColourId,      juce::Colours::black);
    presetBrowserComboBox.setColour (juce::ComboBox::outlineColourId,    juce::Colours::black);
    presetBrowserComboBox.onChange = [this] { loadPreset (presetBrowserComboBox.getSelectedId()); };
    presetBrowserComboBox.setVisible (false);
    refreshPresetList();

    addAndMakeVisible (savePresetButton);
    savePresetButton.setButtonText ("SAVE");
    savePresetButton.setColour (juce::TextButton::buttonColourId,  juce::Colours::white);
    savePresetButton.setColour (juce::TextButton::textColourOffId, juce::Colours::blue);
    savePresetButton.onClick = [this] { savePreset(); };
    savePresetButton.setVisible (false);

    addAndMakeVisible (deletePresetButton);
    deletePresetButton.setButtonText ("DELETE");
    deletePresetButton.setColour (juce::TextButton::buttonColourId,  juce::Colours::white);
    deletePresetButton.setColour (juce::TextButton::textColourOffId, juce::Colours::red);
    deletePresetButton.onClick = [this] { deletePreset(); };
    deletePresetButton.setVisible (false);

    addAndMakeVisible (prevPresetButton);
    prevPresetButton.setButtonText ("<");
    prevPresetButton.setColour (juce::TextButton::buttonColourId,  juce::Colours::white);
    prevPresetButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    prevPresetButton.onClick = [this]
    {
        int cur = presetBrowserComboBox.getSelectedId();
        int num = presetBrowserComboBox.getNumItems();
        if (num < 2) return;                         // nothing beyond "(no preset)"
        int next = (cur <= 2) ? num + 1 : cur - 1;  // id 1 = "(no preset)", wrap from 2 to last
        presetBrowserComboBox.setSelectedId (next, juce::sendNotification);
    };
    prevPresetButton.setVisible (false);

    addAndMakeVisible (nextPresetButton);
    nextPresetButton.setButtonText (">");
    nextPresetButton.setColour (juce::TextButton::buttonColourId,  juce::Colours::white);
    nextPresetButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    nextPresetButton.onClick = [this]
    {
        int cur = presetBrowserComboBox.getSelectedId();
        int num = presetBrowserComboBox.getNumItems();
        if (num < 2) return;
        int next = (cur >= num + 1) ? 2 : cur + 1;  // wrap from last back to first real preset
        if (next == 1) next = 2;                     // skip "(no preset)" slot
        presetBrowserComboBox.setSelectedId (next, juce::sendNotification);
    };
    nextPresetButton.setVisible (false);

    setupGlobalTextBox (ampA0TextBox, "0123456789.", 6,
        [this] { ampA0Applied(); },
        [this] (float delta)
        {
            if (activeAmpSubTab == 1)
            {
                double value = juce::jlimit (0.0, 0.5, ampA0TextBox.getText().getDoubleValue() + delta * 0.001);
                ampA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            }
            else
            {
                double value = juce::jlimit (0.0, 1.0, ampA0TextBox.getText().getDoubleValue() + delta * 0.01);
                ampA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
                int keyIndex = audioProcessor.lastAppliedKeyVolumeStartKey;
                audioProcessor.keyVolume[keyIndex] = value;
                if (selectedKey == keyIndex)
                    keyVolumeSlider.setValue (value, juce::dontSendNotification);
            }
        });

    setupGlobalTextBox (ampC8TextBox, "0123456789.", 6,
        [this] { ampC8Applied(); },
        [this] (float delta)
        {
            if (activeAmpSubTab == 1)
            {
                double value = juce::jlimit (0.0, 0.5, ampC8TextBox.getText().getDoubleValue() + delta * 0.001);
                ampC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            }
            else
            {
                double value = juce::jlimit (0.0, 1.0, ampC8TextBox.getText().getDoubleValue() + delta * 0.01);
                ampC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
                int keyIndex = audioProcessor.lastAppliedKeyVolumeEndKey;
                audioProcessor.keyVolume[keyIndex] = value;
                if (selectedKey == keyIndex)
                    keyVolumeSlider.setValue (value, juce::dontSendNotification);
            }
        });
    ampC8TextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::blue);
    ampC8TextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::blue);
    
    
    

    addAndMakeVisible (ampA0UpperTextBox);
    ampA0UpperTextBox.setInputRestrictions (2, "0123456789");
    ampA0UpperTextBox.setJustification (juce::Justification::centred);
    ampA0UpperTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    ampA0UpperTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    ampA0UpperTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    ampA0UpperTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    ampA0UpperTextBox.setVisible (false);
    {
        auto applyAmpA0UpperBox = [this]
        {
            int value = juce::jlimit (1, 88, ampA0UpperTextBox.getText().getIntValue());
            ampA0UpperTextBox.setText (juce::String (value), juce::dontSendNotification);
            if (activeAmpSubTab == 1)
            {
                audioProcessor.lastAppliedAmpStartKey = value - 1;
                for (int key = 0; key <= value - 1; ++key)
                {
                    applyPulseWaveToKey (audioProcessor, key, ampA0TextBox.getText().getDoubleValue());
                    recalculateKeyVolume (key);
                }
            }
            else
            {
                audioProcessor.lastAppliedKeyVolumeStartKey = value - 1;
                double kv = audioProcessor.keyVolume[value - 1];
                ampA0TextBox.setText (juce::String (kv, 4), juce::dontSendNotification);
            }
            repaintGraphArea();
            if (activeAmpSubTab == 1 && selectedKey >= 0 && selectedKey <= value - 1) rebuildAmplitudeSliders();
        };
        ampA0UpperTextBox.onReturnKey = [this, applyAmpA0UpperBox]
        {
            applyAmpA0UpperBox();
            unfocusAllComponents();
        };
        ampA0UpperTextBox.onFocusLost = [applyAmpA0UpperBox] { applyAmpA0UpperBox(); };
    }

    addAndMakeVisible (ampC8UpperTextBox);
    ampC8UpperTextBox.setInputRestrictions (2, "0123456789");
    ampC8UpperTextBox.setJustification (juce::Justification::centred);
    ampC8UpperTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    ampC8UpperTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    ampC8UpperTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::blue);
    ampC8UpperTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::blue);
    ampC8UpperTextBox.setVisible (false);
    auto applyAmpC8UpperBox = [this]
    {
        int value = juce::jlimit (1, 88, ampC8UpperTextBox.getText().getIntValue());
        ampC8UpperTextBox.setText (juce::String (value), juce::dontSendNotification);
        if (activeAmpSubTab == 1)
        {
            audioProcessor.lastAppliedAmpEndKey = value - 1;
            for (int key = value - 1; key < 88; ++key)
            {
                applyPulseWaveToKey (audioProcessor, key, ampC8TextBox.getText().getDoubleValue());
                recalculateKeyVolume (key);
            }
        }
        else
        {
            audioProcessor.lastAppliedKeyVolumeEndKey = value - 1;
            double kv = audioProcessor.keyVolume[value - 1];
            ampC8TextBox.setText (juce::String (kv, 4), juce::dontSendNotification);
        }
        repaint();
        if (activeAmpSubTab == 1 && selectedKey >= value - 1) rebuildAmplitudeSliders();
    };
    ampC8UpperTextBox.onReturnKey = [this, applyAmpC8UpperBox]
    {
        applyAmpC8UpperBox();
        unfocusAllComponents();
    };
    ampC8UpperTextBox.onFocusLost = [applyAmpC8UpperBox] { applyAmpC8UpperBox(); };

    addAndMakeVisible (evenMorphA0UpperTextBox);
    evenMorphA0UpperTextBox.setInputRestrictions (2, "0123456789");
    evenMorphA0UpperTextBox.setJustification (juce::Justification::centred);
    evenMorphA0UpperTextBox.setColour (juce::TextEditor::textColourId,          juce::Colours::black);
    evenMorphA0UpperTextBox.setColour (juce::TextEditor::backgroundColourId,    juce::Colours::white);
    evenMorphA0UpperTextBox.setColour (juce::TextEditor::outlineColourId,       juce::Colours::black);
    evenMorphA0UpperTextBox.setColour (juce::TextEditor::focusedOutlineColourId,juce::Colours::black);
    evenMorphA0UpperTextBox.setVisible (false);
    evenMorphA0UpperTextBox.setText (juce::String (audioProcessor.lastAppliedEvenMorphStartKey + 1), juce::dontSendNotification);
    {
        auto apply = [this]
        {
            int value = juce::jlimit (1, 88, evenMorphA0UpperTextBox.getText().getIntValue());
            evenMorphA0UpperTextBox.setText (juce::String (value), juce::dontSendNotification);
            audioProcessor.lastAppliedEvenMorphStartKey = value - 1;
            for (int key = 0; key <= value - 1; ++key)
            {
                audioProcessor.evenMorphStrength[key] = 1.0;
                int base  = audioProcessor.keyStartIndex[key];
                int count = audioProcessor.harmonicCounts[key];
                for (int h = 0; h < count; ++h)
                {
                    int n = audioProcessor.oscillators[base + h].harmonicNumber;
                    audioProcessor.oscillators[base + h].amplitude = 1.0 / n;
                }
                recalculateKeyVolume (key);
            }
            repaintGraphArea();
            if (selectedKey != -1) rebuildAmplitudeSliders();
        };
        evenMorphA0UpperTextBox.onReturnKey = [this, apply] { apply(); unfocusAllComponents(); };
        evenMorphA0UpperTextBox.onFocusLost = [apply] { apply(); };
    }

    addAndMakeVisible (evenMorphC8UpperTextBox);
    evenMorphC8UpperTextBox.setInputRestrictions (2, "0123456789");
    evenMorphC8UpperTextBox.setJustification (juce::Justification::centred);
    evenMorphC8UpperTextBox.setColour (juce::TextEditor::textColourId,          juce::Colours::black);
    evenMorphC8UpperTextBox.setColour (juce::TextEditor::backgroundColourId,    juce::Colours::white);
    evenMorphC8UpperTextBox.setColour (juce::TextEditor::outlineColourId,       juce::Colours::black);
    evenMorphC8UpperTextBox.setColour (juce::TextEditor::focusedOutlineColourId,juce::Colours::black);
    evenMorphC8UpperTextBox.setVisible (false);
    evenMorphC8UpperTextBox.setText (juce::String (audioProcessor.lastAppliedEvenMorphEndKey + 1), juce::dontSendNotification);
    {
        auto apply = [this]
        {
            int value = juce::jlimit (1, 88, evenMorphC8UpperTextBox.getText().getIntValue());
            evenMorphC8UpperTextBox.setText (juce::String (value), juce::dontSendNotification);
            audioProcessor.lastAppliedEvenMorphEndKey = value - 1;
            for (int key = value - 1; key < 88; ++key)
            {
                audioProcessor.evenMorphStrength[key] = 0.0;
                int base  = audioProcessor.keyStartIndex[key];
                int count = audioProcessor.harmonicCounts[key];
                for (int h = 0; h < count; ++h)
                {
                    int n = audioProcessor.oscillators[base + h].harmonicNumber;
                    if (n % 2 == 0)
                        audioProcessor.oscillators[base + h].amplitude = 0.0;
                    else
                        audioProcessor.oscillators[base + h].amplitude = 1.0 / n;
                }
                recalculateKeyVolume (key);
            }
            repaintGraphArea();
            if (selectedKey != -1) rebuildAmplitudeSliders();
        };
        evenMorphC8UpperTextBox.onReturnKey = [this, apply] { apply(); unfocusAllComponents(); };
        evenMorphC8UpperTextBox.onFocusLost = [apply] { apply(); };
    }

    addAndMakeVisible (evenMorphA0CenterTextBox);
    evenMorphA0CenterTextBox.setInputRestrictions (2, "0123456789");
    evenMorphA0CenterTextBox.setJustification (juce::Justification::centred);
    evenMorphA0CenterTextBox.setColour (juce::TextEditor::textColourId,          juce::Colours::black);
    evenMorphA0CenterTextBox.setColour (juce::TextEditor::backgroundColourId,    juce::Colours::white);
    evenMorphA0CenterTextBox.setColour (juce::TextEditor::outlineColourId,       juce::Colours::black);
    evenMorphA0CenterTextBox.setColour (juce::TextEditor::focusedOutlineColourId,juce::Colours::black);
    evenMorphA0CenterTextBox.setVisible (false);
    evenMorphA0CenterTextBox.setText (juce::String (audioProcessor.lastAppliedNToNSquaredStartKey + 1), juce::dontSendNotification);
    {
        auto apply = [this]
        {
            int value = juce::jlimit (1, 88, evenMorphA0CenterTextBox.getText().getIntValue());
            evenMorphA0CenterTextBox.setText (juce::String (value), juce::dontSendNotification);
            audioProcessor.lastAppliedNToNSquaredStartKey = value - 1;
            for (int key = 0; key <= value - 1; ++key)
            {
                int base  = audioProcessor.keyStartIndex[key];
                int count = audioProcessor.harmonicCounts[key];
                for (int h = 0; h < count; ++h)
                {
                    int n = audioProcessor.oscillators[base + h].harmonicNumber;
                    audioProcessor.oscillators[base + h].amplitude = 1.0 / n;
                }
                audioProcessor.evenMorphStrength[key] = 0.0;
                recalculateKeyVolume (key);
            }
            repaintGraphArea();
            if (selectedKey != -1) rebuildAmplitudeSliders();
        };
        evenMorphA0CenterTextBox.onReturnKey = [this, apply] { apply(); unfocusAllComponents(); };
        evenMorphA0CenterTextBox.onFocusLost = [apply] { apply(); };
    }

    addAndMakeVisible (evenMorphC8CenterTextBox);
    evenMorphC8CenterTextBox.setInputRestrictions (2, "0123456789");
    evenMorphC8CenterTextBox.setJustification (juce::Justification::centred);
    evenMorphC8CenterTextBox.setColour (juce::TextEditor::textColourId,          juce::Colours::black);
    evenMorphC8CenterTextBox.setColour (juce::TextEditor::backgroundColourId,    juce::Colours::white);
    evenMorphC8CenterTextBox.setColour (juce::TextEditor::outlineColourId,       juce::Colours::black);
    evenMorphC8CenterTextBox.setColour (juce::TextEditor::focusedOutlineColourId,juce::Colours::black);
    evenMorphC8CenterTextBox.setVisible (false);
    evenMorphC8CenterTextBox.setText (juce::String (audioProcessor.lastAppliedNToNSquaredEndKey + 1), juce::dontSendNotification);
    {
        auto apply = [this]
        {
            int value = juce::jlimit (1, 88, evenMorphC8CenterTextBox.getText().getIntValue());
            evenMorphC8CenterTextBox.setText (juce::String (value), juce::dontSendNotification);
            audioProcessor.lastAppliedNToNSquaredEndKey = value - 1;
            for (int key = value - 1; key < 88; ++key)
            {
                int base  = audioProcessor.keyStartIndex[key];
                int count = audioProcessor.harmonicCounts[key];
                for (int h = 0; h < count; ++h)
                {
                    int n = audioProcessor.oscillators[base + h].harmonicNumber;
                    audioProcessor.oscillators[base + h].amplitude = 1.0 / (n * n);
                }
                audioProcessor.evenMorphStrength[key] = 1.0;
                recalculateKeyVolume (key);
            }
            repaintGraphArea();
            if (selectedKey != -1) rebuildAmplitudeSliders();
        };
        evenMorphC8CenterTextBox.onReturnKey = [this, apply] { apply(); unfocusAllComponents(); };
        evenMorphC8CenterTextBox.onFocusLost = [apply] { apply(); };
    }

    setupGlobalTextBox (globalTuningTextBox, "-0123456789", 5,
                [this] { globalTuningApplied(); },
                [this] (float) {});
    
    
    
    setupGlobalTextBox (stretchA0TextBox, "-0123456789.", 8,
                    [this] { stretchA0Applied(); },
                    [this] (float) {});

        setupGlobalTextBox (stretchC8TextBox, "-0123456789.", 8,
                    [this] { stretchC8Applied(); },
                    [this] (float) {});
    
    addAndMakeVisible (linearShapeButton);
        linearShapeButton.setButtonText ("1");
    linearShapeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    
    addAndMakeVisible (cubicShapeButton);
        cubicShapeButton.setButtonText ("3");
        cubicShapeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
        cubicShapeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        cubicShapeButton.setVisible (false);
    
    addAndMakeVisible (squaredShapeButton);
        squaredShapeButton.setButtonText ("2");
        squaredShapeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
        squaredShapeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        squaredShapeButton.setVisible (false);
    
    addAndMakeVisible (absValueShapeButton);
        absValueShapeButton.setButtonText ("||");
        absValueShapeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
        absValueShapeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        absValueShapeButton.setVisible (false);
    
    addAndMakeVisible (expShapeButton);
            expShapeButton.setButtonText ("e");
            expShapeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
            expShapeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
            expShapeButton.setVisible (false);

        setupGlobalTextBox (expSteepnessTextBox, "-0123456789", 3,
            [this] { expSteepnessApplied(); },
            [this] (float) {});

    addAndMakeVisible (evenMorphLinearCenterButton);
    evenMorphLinearCenterButton.setButtonText ("1");
    evenMorphLinearCenterButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    evenMorphLinearCenterButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    evenMorphLinearCenterButton.setVisible (false);

    addAndMakeVisible (evenMorphCubicCenterButton);
    evenMorphCubicCenterButton.setButtonText ("3");
    evenMorphCubicCenterButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    evenMorphCubicCenterButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    evenMorphCubicCenterButton.setVisible (false);

    addAndMakeVisible (evenMorphSquaredCenterButton);
    evenMorphSquaredCenterButton.setButtonText ("2");
    evenMorphSquaredCenterButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    evenMorphSquaredCenterButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    evenMorphSquaredCenterButton.setVisible (false);

    addAndMakeVisible (evenMorphAbsValueCenterButton);
    evenMorphAbsValueCenterButton.setButtonText ("||");
    evenMorphAbsValueCenterButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    evenMorphAbsValueCenterButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    evenMorphAbsValueCenterButton.setVisible (false);

    addAndMakeVisible (evenMorphExpCenterButton);
    evenMorphExpCenterButton.setButtonText ("e");
    evenMorphExpCenterButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    evenMorphExpCenterButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    evenMorphExpCenterButton.setVisible (false);

    addAndMakeVisible (evenMorphExpSteepnessCenterTextBox);
    evenMorphExpSteepnessCenterTextBox.setInputRestrictions (3, "-0123456789");
    evenMorphExpSteepnessCenterTextBox.setJustification (juce::Justification::centred);
    evenMorphExpSteepnessCenterTextBox.setColour (juce::TextEditor::textColourId,          juce::Colours::black);
    evenMorphExpSteepnessCenterTextBox.setColour (juce::TextEditor::backgroundColourId,    juce::Colours::white);
    evenMorphExpSteepnessCenterTextBox.setColour (juce::TextEditor::outlineColourId,       juce::Colours::black);
    evenMorphExpSteepnessCenterTextBox.setColour (juce::TextEditor::focusedOutlineColourId,juce::Colours::black);
    evenMorphExpSteepnessCenterTextBox.setVisible (false);
    evenMorphExpSteepnessCenterTextBox.setText (juce::String (audioProcessor.lastAppliedExpKNToNSquared), juce::dontSendNotification);
    {
        auto apply = [this]
        {
            int value = juce::jlimit (-50, 50, evenMorphExpSteepnessCenterTextBox.getText().getIntValue());
            evenMorphExpSteepnessCenterTextBox.setText (juce::String (value), juce::dontSendNotification);
            audioProcessor.lastAppliedExpKNToNSquared = value;
        };
        evenMorphExpSteepnessCenterTextBox.onReturnKey = [this, apply] { apply(); unfocusAllComponents(); };
        evenMorphExpSteepnessCenterTextBox.onFocusLost = [apply] { apply(); };
    }
    
    

    tuningSubTabSliderLookAndFeel.trackThickness   = 1.0f;
    tuningSubTabSliderLookAndFeel.fixedThumbRadius = true;

    addAndMakeVisible (tuningSubTabSlider);
    tuningSubTabSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tuningSubTabSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    tuningSubTabSlider.setRange (1.0, 2.0, 1.0);
    tuningSubTabSlider.setValue (1.0, juce::dontSendNotification);
    tuningSubTabSlider.setColour (juce::Slider::thumbColourId, juce::Colours::black);
    tuningSubTabSlider.setLookAndFeel (&tuningSubTabSliderLookAndFeel);
    tuningSubTabSlider.setVisible (false);
    tuningSubTabSlider.onValueChange = [this]
    {
        int newTab = (int) tuningSubTabSlider.getValue();
        if (newTab == activeTuningSubTab) return;
        unfocusAllComponents();
        activeTuningSubTab = newTab;
        int osc0_t  = audioProcessor.keyStartIndex[0];
        int osc87_t = audioProcessor.keyStartIndex[87];
        if (activeTuningSubTab == 1)
        {
            stretchA0TextBox.setText (juce::String (audioProcessor.oscillators[osc0_t].stretchCents),  juce::dontSendNotification);
            stretchC8TextBox.setText (juce::String (audioProcessor.oscillators[osc87_t].stretchCents), juce::dontSendNotification);
            expSteepnessTextBox.setText (juce::String (audioProcessor.lastAppliedExpKTuning1), juce::dontSendNotification);
        }
        else
        {
            stretchA0TextBox.setText (juce::String (audioProcessor.keyInharmonicityB[0],  4), juce::dontSendNotification);
            stretchC8TextBox.setText (juce::String (audioProcessor.keyInharmonicityB[87], 4), juce::dontSendNotification);
            expSteepnessTextBox.setText (juce::String (audioProcessor.lastAppliedExpKTuning2), juce::dontSendNotification);
        }
        wireShapeButtons();
        repaint();
    };
    
        linearShapeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
        linearShapeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        linearShapeButton.setVisible (false);
    
        setupGlobalTextBox (globalPhaseTextBox, "0123456789", 3,
                        
                        
        [this] { globalPhaseApplied(); },
        [this] (float delta)
        {
            int value = juce::jlimit (1, 360, audioProcessor.globalPhaseValue + (delta > 0 ? 1 : -1));
            
            globalPhaseTextBox.setText (juce::String (value), juce::dontSendNotification);
            audioProcessor.globalPhaseValue = value;
            for (auto& osc : audioProcessor.oscillators)
            {
                osc.startPhaseDegrees = value;
                osc.setStartPhaseDegrees = value;
                osc.startPhase = value * (juce::MathConstants<double>::pi / 180.0);
                osc.needsPhaseUpdate = true;
            }
            recalculateAllKeyVolumes();
            repaintGraphArea();
        });

    setupGlobalTextBox (phaseA0TextBox, "0123456789", 3,
        [this]
        {
            int value = juce::jlimit (1, 360, phaseA0TextBox.getText().getIntValue());
            phaseA0TextBox.setText (juce::String (value), juce::dontSendNotification);
            int start = audioProcessor.keyStartIndex[0];
            int count = audioProcessor.harmonicCounts[0];
            for (int h = 0; h < count; ++h)
            {
                auto& osc = audioProcessor.oscillators[start + h];
                osc.startPhaseDegrees = value;
                osc.setStartPhaseDegrees = value;
                osc.startPhase = value * (juce::MathConstants<double>::pi / 180.0);
                osc.needsPhaseUpdate = true;
            }
            audioProcessor.lastAppliedPhaseA0 = value;
            recalculateKeyVolume (0);
            repaintGraphArea();
            phaseA0TextBox.giveAwayKeyboardFocus();
        },
        [] (float) {});

    setupGlobalTextBox (phaseC8TextBox, "0123456789", 3,
        [this]
        {
            int value = juce::jlimit (1, 360, phaseC8TextBox.getText().getIntValue());
            phaseC8TextBox.setText (juce::String (value), juce::dontSendNotification);
            int start = audioProcessor.keyStartIndex[87];
            int count = audioProcessor.harmonicCounts[87];
            for (int h = 0; h < count; ++h)
            {
                auto& osc = audioProcessor.oscillators[start + h];
                osc.startPhaseDegrees = value;
                osc.setStartPhaseDegrees = value;
                osc.startPhase = value * (juce::MathConstants<double>::pi / 180.0);
                osc.needsPhaseUpdate = true;
            }
            audioProcessor.lastAppliedPhaseC8 = value;
            recalculateKeyVolume (87);
            repaintGraphArea();
            phaseC8TextBox.giveAwayKeyboardFocus();
        },
        [] (float) {});

    setupGlobalTextBox (globalPanTextBox, "-0123456789.", 6,
        [this] { globalPanApplied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (-1.0, 1.0, audioProcessor.globalPanValue + delta * 0.01);
            globalPanTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.globalPanValue = value;
            for (auto& osc : audioProcessor.oscillators)
            {
                osc.pan = value;
                osc.needsPanUpdate = true;
            }
        });

    setupGlobalTextBox (panWidthTextBox, "-0123456789.", 6,
        [this]
        {
            double value = juce::jlimit (-1.0, 1.0, panWidthTextBox.getText().getDoubleValue());
            panWidthTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.lastAppliedPanWidth = value;
            panWidthTextBox.giveAwayKeyboardFocus();
        },
        [this] (float delta)
        {
            double value = juce::jlimit (-1.0, 1.0, panWidthTextBox.getText().getDoubleValue() + delta * 0.01);
            panWidthTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.lastAppliedPanWidth = value;
        });

    setupGlobalTextBox (globalReleaseTextBox, "0123456789.", 7,
            [this] { globalReleaseApplied(); },
            [this] (float delta)
            {
                double value = juce::jlimit (0.0, 1.0, audioProcessor.globalReleaseValue + delta * 0.001);
                globalReleaseTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
                audioProcessor.globalReleaseValue = value;
                for (auto& osc : audioProcessor.oscillators)
                    osc.releaseTime = osc.setReleaseTime = value;
            });

    setupGlobalTextBox (releaseA0TextBox, "0123456789.", 6,
        [this] { releaseA0Applied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 1.0, releaseA0TextBox.getText().getDoubleValue() + delta * 0.001);
            releaseA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            applyShapeToRelease ([] (double t) { return (t + 1.0) / 2.0; });
        });

    setupGlobalTextBox (releaseC8TextBox, "0123456789.", 6,
        [this] { releaseC8Applied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 1.0, releaseC8TextBox.getText().getDoubleValue() + delta * 0.001);
            releaseC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            applyShapeToRelease ([] (double t) { return (t + 1.0) / 2.0; });
        });

    setupGlobalTextBox (globalSustainTextBox, "0123456789.", 6,
        [this] { globalSustainApplied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 1.0, audioProcessor.globalSustainValue + delta * 0.01);
            globalSustainTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.globalSustainValue = value;
            for (auto& osc : audioProcessor.oscillators)
                osc.sustainLevel = value;
            repaintGraphArea();
        });

    // sustainA0UpperTextBox — start key (black outline, matches ampA0UpperTextBox)
    addAndMakeVisible (sustainA0UpperTextBox);
    sustainA0UpperTextBox.setInputRestrictions (2, "0123456789");
    sustainA0UpperTextBox.setJustification (juce::Justification::centred);
    sustainA0UpperTextBox.setColour (juce::TextEditor::textColourId,           juce::Colours::black);
    sustainA0UpperTextBox.setColour (juce::TextEditor::backgroundColourId,     juce::Colours::white);
    sustainA0UpperTextBox.setColour (juce::TextEditor::outlineColourId,        juce::Colours::black);
    sustainA0UpperTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    sustainA0UpperTextBox.setVisible (false);
    {
        auto applySustainA0Upper = [this] {
            int key = juce::jlimit (1, 88, sustainA0UpperTextBox.getText().getIntValue());
            sustainA0UpperTextBox.setText (juce::String (key), juce::dontSendNotification);
            audioProcessor.lastAppliedSustainStartKey = key - 1;
            double actual = audioProcessor.oscillators[audioProcessor.keyStartIndex[key - 1]].sustainLevel;
            sustainA0TextBox.setText (juce::String (actual, 4), juce::dontSendNotification);
        };
        sustainA0UpperTextBox.onFocusLost = [applySustainA0Upper] { applySustainA0Upper(); };
        sustainA0UpperTextBox.onReturnKey = [this, applySustainA0Upper] {
            applySustainA0Upper();
            unfocusAllComponents();
        };
    }

    // sustainA0TextBox — A0 sustain value (black outline, ScrollableTextEditor, matches ampA0TextBox)
    setupGlobalTextBox (sustainA0TextBox, "0123456789.", 6,
        [this] {
            double v = juce::jlimit (0.0, 1.0, sustainA0TextBox.getText().getDoubleValue());
            sustainA0TextBox.setText (juce::String (v, 4), juce::dontSendNotification);
            int endKey = audioProcessor.lastAppliedSustainStartKey;
            for (int key = 0; key <= endKey; ++key)
            {
                int si = audioProcessor.keyStartIndex[key];
                for (int h = 0; h < audioProcessor.harmonicCounts[key]; ++h)
                    audioProcessor.oscillators[si + h].sustainLevel = v;
            }
            audioProcessor.lastAppliedSustainA0 = v;
            repaintGraphArea();
            if (selectedKey != -1) rebuildSustainSliders();
        },
        [this] (float delta) {
            double v = juce::jlimit (0.0, 1.0, sustainA0TextBox.getText().getDoubleValue() + delta * 0.01);
            sustainA0TextBox.setText (juce::String (v, 4), juce::dontSendNotification);
            int endKey = audioProcessor.lastAppliedSustainStartKey;
            for (int key = 0; key <= endKey; ++key)
            {
                int si = audioProcessor.keyStartIndex[key];
                for (int h = 0; h < audioProcessor.harmonicCounts[key]; ++h)
                    audioProcessor.oscillators[si + h].sustainLevel = v;
            }
            audioProcessor.lastAppliedSustainA0 = v;
            repaintGraphArea();
        });

    // sustainC8UpperTextBox — end key (blue outline, matches ampC8UpperTextBox)
    addAndMakeVisible (sustainC8UpperTextBox);
    sustainC8UpperTextBox.setInputRestrictions (2, "0123456789");
    sustainC8UpperTextBox.setJustification (juce::Justification::centred);
    sustainC8UpperTextBox.setColour (juce::TextEditor::textColourId,           juce::Colours::black);
    sustainC8UpperTextBox.setColour (juce::TextEditor::backgroundColourId,     juce::Colours::white);
    sustainC8UpperTextBox.setColour (juce::TextEditor::outlineColourId,        juce::Colours::blue);
    sustainC8UpperTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::blue);
    sustainC8UpperTextBox.setVisible (false);
    {
        auto applySustainC8Upper = [this] {
            int key = juce::jlimit (1, 88, sustainC8UpperTextBox.getText().getIntValue());
            sustainC8UpperTextBox.setText (juce::String (key), juce::dontSendNotification);
            audioProcessor.lastAppliedSustainEndKey = key - 1;
            double actual = audioProcessor.oscillators[audioProcessor.keyStartIndex[key - 1]].sustainLevel;
            sustainC8TextBox.setText (juce::String (actual, 4), juce::dontSendNotification);
        };
        sustainC8UpperTextBox.onFocusLost = [applySustainC8Upper] { applySustainC8Upper(); };
        sustainC8UpperTextBox.onReturnKey = [this, applySustainC8Upper] {
            applySustainC8Upper();
            unfocusAllComponents();
        };
    }

    // sustainC8TextBox — C8 sustain value (blue outline, ScrollableTextEditor, matches ampC8TextBox)
    setupGlobalTextBox (sustainC8TextBox, "0123456789.", 6,
        [this] {
            double v = juce::jlimit (0.0, 1.0, sustainC8TextBox.getText().getDoubleValue());
            sustainC8TextBox.setText (juce::String (v, 4), juce::dontSendNotification);
            int startKey = audioProcessor.lastAppliedSustainEndKey;
            for (int key = startKey; key < 88; ++key)
            {
                int si = audioProcessor.keyStartIndex[key];
                for (int h = 0; h < audioProcessor.harmonicCounts[key]; ++h)
                    audioProcessor.oscillators[si + h].sustainLevel = v;
            }
            audioProcessor.lastAppliedSustainC8 = v;
            repaintGraphArea();
            if (selectedKey != -1) rebuildSustainSliders();
        },
        [this] (float delta) {
            double v = juce::jlimit (0.0, 1.0, sustainC8TextBox.getText().getDoubleValue() + delta * 0.01);
            sustainC8TextBox.setText (juce::String (v, 4), juce::dontSendNotification);
            int startKey = audioProcessor.lastAppliedSustainEndKey;
            for (int key = startKey; key < 88; ++key)
            {
                int si = audioProcessor.keyStartIndex[key];
                for (int h = 0; h < audioProcessor.harmonicCounts[key]; ++h)
                    audioProcessor.oscillators[si + h].sustainLevel = v;
            }
            audioProcessor.lastAppliedSustainC8 = v;
            repaintGraphArea();
        });
    sustainC8TextBox.setColour (juce::TextEditor::outlineColourId,        juce::Colours::blue);
    sustainC8TextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::blue);

    for (auto* btn : { &sustainLinearButton, &sustainSquaredButton, &sustainCubicButton,
                       &sustainAbsValueButton, &sustainExpButton })
    {
        addAndMakeVisible (btn);
        btn->setColour (juce::TextButton::buttonColourId,  juce::Colours::white);
        btn->setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        btn->setVisible (false);
    }
    sustainLinearButton.setButtonText   ("1");
    sustainSquaredButton.setButtonText  ("2");
    sustainCubicButton.setButtonText    ("3");
    sustainAbsValueButton.setButtonText ("||");
    sustainExpButton.setButtonText      ("e");

    setupGlobalTextBox (sustainExpKTextBox, "-0123456789", 3,
        [this] {
            int v = juce::jlimit (-50, 50, sustainExpKTextBox.getText().getIntValue());
            sustainExpKTextBox.setText (juce::String (v), juce::dontSendNotification);
            audioProcessor.lastAppliedExpKSustain = v;
        },
        [this] (float) {});
    sustainExpKTextBox.setVisible (false);

    setupGlobalTextBox (globalDecayTextBox, "0123456789.", 7,
        [this] { globalDecayApplied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 20.0, audioProcessor.globalDecayValue + delta * 0.01);
            globalDecayTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.globalDecayValue = value;
            for (int key = 0; key < 88; ++key)
            {
                audioProcessor.oscillators[audioProcessor.keyStartIndex[key]].decayTime = value;
                reapplyDecayShapeForKey (key);
            }
        });
    

    decaySubTabSliderLookAndFeel.trackThickness   = 1.0f;
    decaySubTabSliderLookAndFeel.fixedThumbRadius = true;

    addAndMakeVisible (decaySubTabSlider);
    decaySubTabSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    decaySubTabSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    decaySubTabSlider.setRange (1.0, 2.0, 1.0);
    decaySubTabSlider.setValue (1.0, juce::dontSendNotification);
    decaySubTabSlider.setColour (juce::Slider::thumbColourId, juce::Colours::black);
    decaySubTabSlider.setLookAndFeel (&decaySubTabSliderLookAndFeel);
    decaySubTabSlider.setVisible (false);
    decaySubTabSlider.onValueChange = [this]
    {
        int newTab = (int) decaySubTabSlider.getValue();
        if (newTab == activeDecaySubTab) return;
        activeDecaySubTab = newTab;
        if (activeDecaySubTab == 1)
        {
            int osc0_d  = audioProcessor.keyStartIndex[0];
            int osc87_d = audioProcessor.keyStartIndex[87];
            decayA0TextBox.setText (juce::String (audioProcessor.oscillators[osc0_d].decayTime,  4), juce::dontSendNotification);
            decayC8TextBox.setText (juce::String (audioProcessor.oscillators[osc87_d].decayTime, 4), juce::dontSendNotification);
            expSteepnessTextBox.setText (juce::String (audioProcessor.lastAppliedExpKDecay1), juce::dontSendNotification);
        }
        else
        {
            expSteepnessTextBox.setText (juce::String (audioProcessor.lastAppliedExpKDecay2), juce::dontSendNotification);
        }
        wireShapeButtons();
        updateControlVisibility();
        repaint();
    };

    setupGlobalTextBox (decayA0TextBox, "0123456789.", 7,
        [this] { decayA0Applied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 10.0, decayA0TextBox.getText().getDoubleValue() + delta * 0.01);
            decayA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.oscillators[audioProcessor.keyStartIndex[0]].decayTime = value;
            reapplyDecayShapeForKey (0);
        });
    setupGlobalTextBox (decayC8TextBox, "0123456789.", 7,
        [this] { decayC8Applied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 10.0, decayC8TextBox.getText().getDoubleValue() + delta * 0.01);
            decayC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.oscillators[audioProcessor.keyStartIndex[87]].decayTime = value;
            reapplyDecayShapeForKey (87);
        });

    setupGlobalTextBox (decayIIA0TextBox, "0123456789.", 6,
        [this]
        {
            double value = juce::jlimit (0.0, 1.0, decayIIA0TextBox.getText().getDoubleValue());
            decayIIA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.lastAppliedDecayIIThreshold = value;
            applyShapeToDecayHarmonics (lastDecayIIShape);
            decayIIA0TextBox.giveAwayKeyboardFocus();
        },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 1.0, audioProcessor.lastAppliedDecayIIThreshold + delta * 0.01);
            decayIIA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.lastAppliedDecayIIThreshold = value;
            applyShapeToDecayHarmonics (lastDecayIIShape);
        });
    decayIIA0TextBox.setVisible (false);

    setupGlobalTextBox (attackA0TextBox, "0123456789.", 6,
        [this] { attackA0Applied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 1.0, attackA0TextBox.getText().getDoubleValue() + delta * 0.01);
            attackA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            int startIndex = audioProcessor.keyStartIndex[0];
            int count = audioProcessor.harmonicCounts[0];
            for (int h = 0; h < count; ++h)
            {
                audioProcessor.oscillators[startIndex + h].attackTime = value;
                audioProcessor.oscillators[startIndex + h].setAttackTime = value;
            }
            repaintGraphArea();
        });
    setupGlobalTextBox (attackC8TextBox, "0123456789.", 6,
        [this] { attackC8Applied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 1.0, attackC8TextBox.getText().getDoubleValue() + delta * 0.01);
            attackC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            int startIndex = audioProcessor.keyStartIndex[87];
            int count = audioProcessor.harmonicCounts[87];
            for (int h = 0; h < count; ++h)
            {
                audioProcessor.oscillators[startIndex + h].attackTime = value;
                audioProcessor.oscillators[startIndex + h].setAttackTime = value;
            }
            repaintGraphArea();
        });
    setupGlobalTextBox (globalAttackTextBox, "0123456789.", 6,
        [this] { globalAttackApplied(); },
        [this] (float delta)
        {
            double value = juce::jlimit (0.0, 1.0, audioProcessor.globalAttackValue + delta * 0.01);
            globalAttackTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
            audioProcessor.globalAttackValue = value;
            for (auto& osc : audioProcessor.oscillators)
            {
                osc.attackTime = value;
                osc.setAttackTime = value;
            }
            repaintGraphArea();
        });

    addAndMakeVisible (attackRandTextBox);
    attackRandTextBox.setInputRestrictions (6, "0123456789.");
    attackRandTextBox.setJustification (juce::Justification::centred);
    attackRandTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    attackRandTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    attackRandTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    attackRandTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    attackRandTextBox.setVisible (false);

    addAndMakeVisible (attackRandLabel);
    attackRandLabel.setText ("RAND", juce::dontSendNotification);
    attackRandLabel.setJustificationType (juce::Justification::centredRight);
    attackRandLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    attackRandLabel.setVisible (false);

    auto applyAttackRand = [this]
    {
        double randValue = juce::jlimit (0.0, 1.0, attackRandTextBox.getText().getDoubleValue());
        attackRandTextBox.setText (juce::String (randValue, 2), juce::dontSendNotification);
        audioProcessor.lastAppliedAttackRand = randValue;

        auto& rng = juce::Random::getSystemRandom();

        if (randValue == 0.0)
        {
            for (auto& osc : audioProcessor.oscillators)
                osc.attackTime = osc.setAttackTime;
        }
        else
        {
            for (auto& osc : audioProcessor.oscillators)
                osc.attackTime = juce::jlimit (0.0, 1.0, osc.setAttackTime + (rng.nextDouble() * 2.0 - 1.0) * randValue * osc.setAttackTime);
        }

        repaintGraphArea();
        unfocusAllComponents();
    };
    attackRandTextBox.onReturnKey = applyAttackRand;
    attackRandTextBox.onFocusLost = [this]
    {
        double randValue = juce::jlimit (0.0, 1.0, attackRandTextBox.getText().getDoubleValue());
        attackRandTextBox.setText (juce::String (randValue, 2), juce::dontSendNotification);
        audioProcessor.lastAppliedAttackRand = randValue;
    };

    addAndMakeVisible (phaseEvensTextBox);
    phaseEvensTextBox.setInputRestrictions (3, "0123456789");
    phaseEvensTextBox.setJustification (juce::Justification::centred);
    phaseEvensTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    phaseEvensTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    phaseEvensTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    phaseEvensTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    phaseEvensTextBox.setText ("1", juce::dontSendNotification);
    phaseEvensTextBox.setVisible (false);

    addAndMakeVisible (phaseEvensLabel);
    phaseEvensLabel.setText ("EVENS PHASE", juce::dontSendNotification);
    phaseEvensLabel.setJustificationType (juce::Justification::centredRight);
    phaseEvensLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    phaseEvensLabel.setVisible (false);

    {
        auto applyPhaseEvens = [this]
        {
            int value = juce::jlimit (1, 360, phaseEvensTextBox.getText().getIntValue());
            phaseEvensTextBox.setText (juce::String (value), juce::dontSendNotification);
            for (int key = 0; key < 88; ++key)
            {
                int start = audioProcessor.keyStartIndex[key];
                int count = audioProcessor.harmonicCounts[key];
                for (int h = 0; h < count; ++h)
                {
                    if ((h + 1) % 2 == 0)
                    {
                        auto& osc = audioProcessor.oscillators[start + h];
                        osc.startPhaseDegrees = value;
                        osc.setStartPhaseDegrees = value;
                        osc.startPhase = value * (juce::MathConstants<double>::pi / 180.0);
                        osc.needsPhaseUpdate = true;
                    }
                }
                recalculateKeyVolume (key);
            }
            repaintGraphArea();
            unfocusAllComponents();
        };
        phaseEvensTextBox.onReturnKey = applyPhaseEvens;
        phaseEvensTextBox.onFocusLost = [this]
        {
            int value = juce::jlimit (1, 360, phaseEvensTextBox.getText().getIntValue());
            phaseEvensTextBox.setText (juce::String (value), juce::dontSendNotification);
        };
    }

    addAndMakeVisible (phaseRandTextBox);
    phaseRandTextBox.setInputRestrictions (6, "0123456789.");
    phaseRandTextBox.setJustification (juce::Justification::centred);
    phaseRandTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    phaseRandTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    phaseRandTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    phaseRandTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    phaseRandTextBox.setVisible (false);

    addAndMakeVisible (phaseRandLabel);
    phaseRandLabel.setText ("RAND", juce::dontSendNotification);
    phaseRandLabel.setJustificationType (juce::Justification::centredRight);
    phaseRandLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    phaseRandLabel.setVisible (false);

    phaseRandTextBox.onReturnKey = [this]
    {
        double randValue = juce::jlimit (0.0, 1.0, phaseRandTextBox.getText().getDoubleValue());
        phaseRandTextBox.setText (juce::String (randValue, 2), juce::dontSendNotification);
        audioProcessor.lastAppliedPhaseRand = randValue;

        auto& rng = juce::Random::getSystemRandom();

        for (int key = 0; key < 88; ++key)
        {
            int start = audioProcessor.keyStartIndex[key];
            int count = audioProcessor.harmonicCounts[key];
            for (int h = 0; h < count; ++h)
            {
                auto& osc = audioProcessor.oscillators[start + h];
                double base = osc.setStartPhaseDegrees;
                double deviated = base + (rng.nextDouble() * 2.0 - 1.0) * randValue * base;
                int clamped = juce::jlimit (1, 360, (int) std::round (deviated));
                osc.startPhaseDegrees = clamped;
                osc.startPhase = clamped * (juce::MathConstants<double>::pi / 180.0);
                osc.needsPhaseUpdate = true;
            }
            recalculateKeyVolume (key);
        }

        repaint();
        unfocusAllComponents();
    };
    phaseRandTextBox.onFocusLost = [this]
    {
        double randValue = juce::jlimit (0.0, 1.0, phaseRandTextBox.getText().getDoubleValue());
        phaseRandTextBox.setText (juce::String (randValue, 2), juce::dontSendNotification);
        audioProcessor.lastAppliedPhaseRand = randValue;
    };

    addAndMakeVisible (releaseRandTextBox);
    releaseRandTextBox.setInputRestrictions (6, "0123456789.");
    releaseRandTextBox.setJustification (juce::Justification::centred);
    releaseRandTextBox.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    releaseRandTextBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    releaseRandTextBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    releaseRandTextBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);
    releaseRandTextBox.setVisible (false);

    addAndMakeVisible (releaseRandLabel);
    releaseRandLabel.setText ("RAND", juce::dontSendNotification);
    releaseRandLabel.setJustificationType (juce::Justification::centredRight);
    releaseRandLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    releaseRandLabel.setVisible (false);

    auto applyReleaseRand = [this]
    {
        double randValue = juce::jlimit (0.0, 1.0, releaseRandTextBox.getText().getDoubleValue());
        releaseRandTextBox.setText (juce::String (randValue, 2), juce::dontSendNotification);
        audioProcessor.lastAppliedReleaseRand = randValue;

        auto& rng = juce::Random::getSystemRandom();

        if (randValue == 0.0)
        {
            for (auto& osc : audioProcessor.oscillators)
                osc.releaseTime = osc.setReleaseTime;
        }
        else
        {
            for (auto& osc : audioProcessor.oscillators)
                osc.releaseTime = juce::jlimit (0.0, 1.0, osc.setReleaseTime + (rng.nextDouble() * 2.0 - 1.0) * randValue * osc.setReleaseTime);
        }

        repaintGraphArea();
        unfocusAllComponents();
    };
    releaseRandTextBox.onReturnKey = applyReleaseRand;
    releaseRandTextBox.onFocusLost = [this]
    {
        double randValue = juce::jlimit (0.0, 1.0, releaseRandTextBox.getText().getDoubleValue());
        releaseRandTextBox.setText (juce::String (randValue, 2), juce::dontSendNotification);
        audioProcessor.lastAppliedReleaseRand = randValue;
    };

    addAndMakeVisible (tuningViewport);
    tuningViewport.setViewedComponent (&tuningContainer, false);
    tuningViewport.setScrollBarsShown (false, true);
    tuningViewport.setVisible (false);

    addAndMakeVisible (phaseViewport);
    phaseViewport.setViewedComponent (&phaseContainer, false);
    phaseViewport.setScrollBarsShown (false, true);
    phaseViewport.setVisible (false);
    
    
    
    addAndMakeVisible (panViewport);
    panViewport.setViewedComponent (&panContainer, false);
    panViewport.setScrollBarsShown (true, false);
    panViewport.setVisible (false);
    
    addAndMakeVisible (attackViewport);
    attackViewport.setViewedComponent (&attackContainer, false);
    attackViewport.setScrollBarsShown (true, false);
    attackViewport.setVisible (false);
    
    
    addAndMakeVisible (decayViewport);
    decayViewport.setViewedComponent (&decayContainer, false);
    decayViewport.setScrollBarsShown (true, false);
    decayViewport.setVisible (false);

    addAndMakeVisible (sustainViewport);
        sustainViewport.setViewedComponent (&sustainContainer, false);
        sustainViewport.setScrollBarsShown (true, false);
        sustainViewport.setVisible (false);

        addAndMakeVisible (releaseViewport);
        releaseViewport.setViewedComponent (&releaseContainer, false);
        releaseViewport.setScrollBarsShown (true, false);
        releaseViewport.setVisible (false);
    
    

    globalToggleCheckbox.setToggleState (audioProcessor.globalToggleState, juce::dontSendNotification);
            normalizationCheckbox.setToggleState (audioProcessor.normalizationEnabled, juce::dontSendNotification);
            globalAmpTextBox.setText (juce::String (audioProcessor.globalAmpValue, 4), juce::dontSendNotification);
            globalTuningTextBox.setText (juce::String (audioProcessor.globalTuningValue), juce::dontSendNotification);
            globalPhaseTextBox.setText (juce::String (audioProcessor.globalPhaseValue), juce::dontSendNotification);
            globalPanTextBox.setText (juce::String (audioProcessor.globalPanValue, 4), juce::dontSendNotification);
            globalAttackTextBox.setText (juce::String (audioProcessor.globalAttackValue, 4), juce::dontSendNotification);
            globalSustainTextBox.setText (juce::String (audioProcessor.globalSustainValue, 4), juce::dontSendNotification);
    {
        int osc0  = audioProcessor.keyStartIndex[0];
        int osc87 = audioProcessor.keyStartIndex[87];
        sustainA0UpperTextBox.setText (juce::String (audioProcessor.lastAppliedSustainStartKey + 1), juce::dontSendNotification);
        sustainA0TextBox.setText      (juce::String (audioProcessor.lastAppliedSustainA0,  4), juce::dontSendNotification);
        sustainC8UpperTextBox.setText (juce::String (audioProcessor.lastAppliedSustainEndKey + 1),   juce::dontSendNotification);
        sustainC8TextBox.setText      (juce::String (audioProcessor.lastAppliedSustainC8, 4), juce::dontSendNotification);
        sustainExpKTextBox.setText    (juce::String (audioProcessor.lastAppliedExpKSustain),           juce::dontSendNotification);
        globalDecayTextBox.setText   (juce::String (audioProcessor.globalDecayValue,   4), juce::dontSendNotification);
        globalReleaseTextBox.setText (juce::String (audioProcessor.globalReleaseValue, 4), juce::dontSendNotification);
        stretchA0TextBox.setText (juce::String (audioProcessor.lastAppliedStretchA0),  juce::dontSendNotification);
        stretchC8TextBox.setText (juce::String (audioProcessor.lastAppliedStretchC8), juce::dontSendNotification);
        ampA0TextBox.setText (juce::String (audioProcessor.lastAppliedAmpA0,  4), juce::dontSendNotification);
        ampC8TextBox.setText (juce::String (audioProcessor.lastAppliedAmpC8, 4), juce::dontSendNotification);
        ampA0UpperTextBox.setText      (juce::String (audioProcessor.lastAppliedAmpStartKey + 1), juce::dontSendNotification);
        ampC8UpperTextBox.setText      (juce::String (audioProcessor.lastAppliedAmpEndKey + 1), juce::dontSendNotification);
        evenMorphA0UpperTextBox.setText (juce::String (audioProcessor.lastAppliedEvenMorphStartKey + 1), juce::dontSendNotification);
        evenMorphC8UpperTextBox.setText (juce::String (audioProcessor.lastAppliedEvenMorphEndKey   + 1), juce::dontSendNotification);
        evenMorphA0CenterTextBox.setText (juce::String (audioProcessor.lastAppliedNToNSquaredStartKey + 1), juce::dontSendNotification);
        evenMorphC8CenterTextBox.setText (juce::String (audioProcessor.lastAppliedNToNSquaredEndKey   + 1), juce::dontSendNotification);
        attackA0TextBox.setText (juce::String (audioProcessor.lastAppliedAttackA0,  4), juce::dontSendNotification);
        attackC8TextBox.setText (juce::String (audioProcessor.lastAppliedAttackC8, 4), juce::dontSendNotification);
        decayA0TextBox.setText  (juce::String (audioProcessor.lastAppliedDecayA0,  4), juce::dontSendNotification);
        decayC8TextBox.setText  (juce::String (audioProcessor.lastAppliedDecayC8, 4), juce::dontSendNotification);
        decayIIA0TextBox.setText (juce::String (audioProcessor.lastAppliedDecayIIThreshold, 4), juce::dontSendNotification);
        releaseA0TextBox.setText (juce::String (audioProcessor.lastAppliedReleaseA0,  4), juce::dontSendNotification);
        releaseC8TextBox.setText (juce::String (audioProcessor.lastAppliedReleaseC8, 4), juce::dontSendNotification);
        phaseA0TextBox.setText (juce::String (audioProcessor.lastAppliedPhaseA0),  juce::dontSendNotification);
        phaseC8TextBox.setText (juce::String (audioProcessor.lastAppliedPhaseC8), juce::dontSendNotification);
        phaseEvensTextBox.setText ("1",   juce::dontSendNotification);
        phaseRandTextBox.setText  (juce::String (audioProcessor.lastAppliedPhaseRand,  2), juce::dontSendNotification);
        attackRandTextBox.setText (juce::String (audioProcessor.lastAppliedAttackRand, 2), juce::dontSendNotification);
        releaseRandTextBox.setText(juce::String (audioProcessor.lastAppliedReleaseRand, 2), juce::dontSendNotification);
        panWidthTextBox.setText (juce::String (audioProcessor.lastAppliedPanWidth, 4), juce::dontSendNotification);
        integralTextBox.setText (juce::String (audioProcessor.lastAppliedIntegralValue, 2), juce::dontSendNotification);
        expSteepnessTextBox.setText (juce::String (audioProcessor.lastAppliedExpKToggle), juce::dontSendNotification);
    }

    wireShapeButtons();
    updateControlVisibility();

    for (auto* child : getChildren())
    {
        if (auto* button = dynamic_cast<juce::Button*> (child))
            button->setWantsKeyboardFocus (false);
    }

    resized();
    toggleGraphViewport.setViewPosition (0, toggleGraphComponent.getHeight());
}

SineLabAudioProcessorEditor::~SineLabAudioProcessorEditor()
{
    audioProcessor.removeChangeListener (this);


    for (auto* slider : amplitudeSliders)
        slider->setLookAndFeel (nullptr);

    for (auto* slider : tuningSliders)
        slider->setLookAndFeel (nullptr);

    for (auto* slider : phaseSliders)
        slider->setLookAndFeel (nullptr);

    for (auto* slider : panSliders)
        slider->setLookAndFeel (nullptr);

    for (auto* slider : attackSliders)
        slider->setLookAndFeel (nullptr);

    for (auto* slider : decaySliders)
            slider->setLookAndFeel (nullptr);

    for (auto* slider : sustainSliders)
                slider->setLookAndFeel (nullptr);

            for (auto* slider : releaseSliders)
                slider->setLookAndFeel (nullptr);

    keyVolumeSlider.setLookAndFeel (nullptr);
    ampSubTabSlider.setLookAndFeel (nullptr);
    tuningSubTabSlider.setLookAndFeel (nullptr);
    decaySubTabSlider.setLookAndFeel (nullptr);
}

void SineLabAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    audioProcessor.updateActiveRanks();
    // tabSelected restores all text boxes from processor state and handles
    // rebuild + visibility + repaint — one path for preset load and host restore
    tabSelected (activeTab);
}

void SineLabAudioProcessorEditor::repaintGraphArea()
{
    repaint();
}

void SineLabAudioProcessorEditor::repaintKeyboard()
{
    auto bounds = getLocalBounds();
    int keyboardTop = bounds.getHeight() - bounds.getHeight() / 8;
    repaint (0, keyboardTop, bounds.getWidth(), bounds.getHeight() - keyboardTop);
}

//==============================================================================
void SineLabAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);

    auto bounds = getLocalBounds();
    double scale = (double) bounds.getWidth() / 792.0;
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    float bottomLineY = bounds.getHeight() - keyboardAreaHeight;
    float topLineY = bounds.getHeight() / 16.0f;
    float tabWidth = bounds.getWidth() / 9.0f;

    g.setColour (juce::Colours::black);
    g.drawLine (0, bottomLineY, (float) bounds.getWidth(), bottomLineY, 1.0f);
    
    
    float rightThirdX = bounds.getWidth() * (3.0f / 4.0f);
        g.drawLine (rightThirdX, topLineY, rightThirdX, bottomLineY, 1.0f);
    
    

    const juce::String tabLabels[9] = { "TOGGLE", "AMP", "TUNE", "PHASE", "PAN", "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
    juce::Rectangle<int> tabAreas[9];

    for (int i = 0; i < 9; ++i)
    {
        juce::Rectangle<int> area ((int) (i * tabWidth), 0, (int) tabWidth, (int) topLineY);
        tabAreas[i] = area;

        g.setColour (juce::Colours::white);
        g.fillRect (area);
        g.setColour (juce::Colours::black);
        g.drawLine ((float) area.getX(), (float) area.getY(), (float) area.getX(), (float) area.getBottom(), 1.0f);
        g.drawLine ((float) area.getRight(), (float) area.getY(), (float) area.getRight(), (float) area.getBottom(), 1.0f);
        g.drawLine ((float) area.getX(), (float) area.getY(), (float) area.getRight(), (float) area.getY(), 1.0f);
        if (activeTab != i)
            g.drawLine ((float) area.getX(), (float) area.getBottom(), (float) area.getRight(), (float) area.getBottom(), 3.0f);
    }

    g.setFont (g.getCurrentFont().withHeight (15.0f * (float)scale));
    for (int i = 0; i < 9; ++i)
    {
        if (activeTab == i)
            g.setColour (juce::Colours::blue);
        else
            g.setColour (juce::Colours::black);

        g.drawText (tabLabels[i], tabAreas[i], juce::Justification::centred, false);
    }

    const juce::String noteLetters[12] = { "C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B" };
    const bool isSharp[12]             = { false, true, false, true, false, false, true, false, true, false, true, false };

    for (int i = 0; i < keyButtons.size(); ++i)
    {
        auto keyBounds = keyButtons[i]->getBounds();

        if (i == selectedKey)
            g.setColour (juce::Colours::cyan);
        else if (i == hoveredKey)
            g.setColour (juce::Colours::cyan.withMultipliedSaturation (0.25f));
        else
            g.setColour (juce::Colours::white);

        g.fillRect (keyBounds);

        g.setColour (juce::Colours::black);
        g.drawRect (keyBounds, 1);

        int noteNumber = i + 21;
                int positionInOctave = noteNumber % 12;
                juce::String letter = noteLetters[positionInOctave];

                int quarterHeight = keyBounds.getHeight() / 4;

                auto firstQuarter = keyBounds.removeFromTop (quarterHeight);
                g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.0f * (float)scale, juce::Font::plain));
                g.drawText (letter, firstQuarter, juce::Justification::centred, false);

                auto secondQuarter = keyBounds.removeFromTop (quarterHeight);
                if (isSharp[positionInOctave])
                {
                    g.setColour (juce::Colours::red);
                    g.drawText ("#", secondQuarter, juce::Justification::centred, false);
                }

                auto thirdQuarter = keyBounds.removeFromTop (quarterHeight);
                int octaveNumber = (noteNumber / 12) - 1;
                g.setColour (juce::Colours::black);
                g.drawText (juce::String (octaveNumber), thirdQuarter, juce::Justification::centred, false);
        
        int keyNumber = i + 1;
                juce::String keyNumberText = juce::String (keyNumber);

                auto textBounds = keyBounds.toFloat();
                juce::Point<float> centre = textBounds.getCentre();

        g.saveState();
                g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, centre.x, centre.y));
        g.setColour (juce::Colours::blue);
                juce::Rectangle<int> rotatedBounds ((int) (centre.x - keyBounds.getHeight() / 2.0f),
                                                     (int) (centre.y - keyBounds.getWidth() / 2.0f),
                                                     keyBounds.getHeight(),
                                                     keyBounds.getWidth());

                g.drawText (keyNumberText, rotatedBounds, juce::Justification::centred, false);
                g.restoreState();
        
        
        
        
    }
    g.setColour (juce::Colours::black);

    if (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1)
    {
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f * (float)scale, juce::Font::plain));
        g.drawText ("NORM", normalizationCheckbox.getBounds().translated (-45, 0), juce::Justification::centredRight, false);
    }
    
    // toggle graph is drawn by toggleGraphComponent inside toggleGraphViewport

    if (selectedKey == -1 && activeTab == 2)
        {
            paintTuningGraph (g);
            int tuningGraphTop    = (int) (bounds.getHeight() / 16.0f);
            int tuningGraphBottom = (int) (bounds.getHeight() - bounds.getHeight() / 8.0f);
            int tuningGraphMidY   = tuningGraphTop + (tuningGraphBottom - tuningGraphTop) / 2;
            int tuningContextX    = (bounds.getWidth() * 3) / 4;
            g.setColour (juce::Colours::blue);
            g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
            g.drawText ("0", tuningContextX + 2, tuningGraphMidY - 9, 20, 18, juce::Justification::centredLeft, false);
        }
    
    if (selectedKey == -1 && activeTab == 5)
    {
        paintAttackGraph (g);
        int contextAreaLeftEdge = (bounds.getWidth() * 3) / 4;
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
        g.setColour (juce::Colours::blue);
        g.drawText ("1s", contextAreaLeftEdge + 2, (int) topLineY, 36, 18, juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::red);
        g.drawText ("0s", contextAreaLeftEdge + 2, (int) (bounds.getHeight() - bounds.getHeight() / 8.0f) - 18, 36, 18, juce::Justification::centredLeft, false);
    }

    if (selectedKey == -1 && activeTab == 7)
    {
        paintSustainGraph (g);
        int contextAreaLeftEdge = (bounds.getWidth() * 3) / 4;
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
        g.setColour (juce::Colours::blue);
        g.drawText ("1", contextAreaLeftEdge + 2, (int) topLineY, 36, 18, juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::red);
        g.drawText ("0", contextAreaLeftEdge + 2, (int) (bounds.getHeight() - bounds.getHeight() / 8.0f) - 18, 36, 18, juce::Justification::centredLeft, false);
    }

    if (selectedKey == -1 && activeTab == 6)
    {
        paintDecayGraph (g);
        int contextAreaLeftEdge = (bounds.getWidth() * 3) / 4;
        int contextAreaWidth = bounds.getWidth() - contextAreaLeftEdge;
        g.setColour (juce::Colours::black);
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
        juce::String decaySubTabLabel = (activeDecaySubTab == 1) ? "KEYBOARD" : "KEY";
        g.drawText (decaySubTabLabel, contextAreaLeftEdge, (int) topLineY, contextAreaWidth, 25, juce::Justification::centred, false);
        if (activeDecaySubTab == 1)
        {
            g.setColour (juce::Colours::blue);
            g.drawText ("10s", contextAreaLeftEdge + 2, (int) topLineY, 36, 18, juce::Justification::centredLeft, false);
            g.setColour (juce::Colours::red);
            g.drawText ("0", contextAreaLeftEdge + 2, (int) (bounds.getHeight() - bounds.getHeight() / 8.0f) - 18, 36, 18, juce::Justification::centredLeft, false);
        }

        {
            auto sb    = decaySubTabSlider.getBounds();
            int thumbR = 5;
            int labelH = 11;
            int labelW = 10;
            int labelY = sb.getY() + sb.getHeight() / 2 - thumbR - labelH;
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f * (float)scale, juce::Font::plain));
            g.drawText ("1", sb.getX(),              labelY, labelW, labelH, juce::Justification::centred, false);
            g.drawText ("2", sb.getRight() - labelW, labelY, labelW, labelH, juce::Justification::centred, false);
        }
    }

    if (selectedKey == -1 && activeTab == 8)
    {
        paintReleaseGraph (g);
        int contextAreaLeftEdge = (bounds.getWidth() * 3) / 4;
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
        g.setColour (juce::Colours::blue);
        g.drawText ("1s", contextAreaLeftEdge + 2, (int) topLineY, 36, 18, juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::red);
        g.drawText ("0s", contextAreaLeftEdge + 2, (int) (bounds.getHeight() - bounds.getHeight() / 8.0f) - 18, 36, 18, juce::Justification::centredLeft, false);
    }

    if (selectedKey == -1 && activeTab == 2)
            {
                int contextAreaLeftEdgeForLabel = (bounds.getWidth() * 3) / 4;
                int contextAreaWidthForLabel = bounds.getWidth() - contextAreaLeftEdgeForLabel;
                g.setColour (juce::Colours::black);
                g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));

                juce::String subTabLabel = (activeTuningSubTab == 1) ? "STRETCH" : "INHARMONICITY";
                g.drawText (subTabLabel, contextAreaLeftEdgeForLabel, (int) topLineY, contextAreaWidthForLabel, 25, juce::Justification::centred, false);

                {
                    auto sb    = tuningSubTabSlider.getBounds();
                    int thumbR = 5;
                    int labelH = 11;
                    int labelW = 10;
                    int labelY = sb.getY() + sb.getHeight() / 2 - thumbR - labelH;
                    g.setColour (juce::Colours::black);
                    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f * (float)scale, juce::Font::plain));
                    g.drawText ("1", sb.getX(),           labelY, labelW, labelH, juce::Justification::centred, false);
                    g.drawText ("2", sb.getRight() - labelW, labelY, labelW, labelH, juce::Justification::centred, false);
                }
            }

    if (selectedKey == -1 && activeTab == 1)
            {
                int contextAreaLeftEdge = (bounds.getWidth() * 3) / 4;
                int contextAreaWidth = bounds.getWidth() - contextAreaLeftEdge;
                g.setColour (juce::Colours::black);
                g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
                if (activeAmpSubTab == 1)
                {
                    g.drawText ("DUTY CYCLE", contextAreaLeftEdge, (int) topLineY, contextAreaWidth, 25, juce::Justification::centred, false);
                    paintAmpGraph (g);
                    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
                    g.setColour (juce::Colours::blue);
                    g.drawText ("50%", contextAreaLeftEdge + 2, (int) topLineY, 36, 18, juce::Justification::centredLeft, false);
                    g.setColour (juce::Colours::red);
                    g.drawText ("0%",  contextAreaLeftEdge + 2, (int) (bounds.getHeight() - bounds.getHeight() / 8.0f) - 18, 36, 18, juce::Justification::centredLeft, false);
                }
                else if (activeAmpSubTab == 2)
                {
                    g.drawText ("KEY VOLUME", contextAreaLeftEdge, (int) topLineY, contextAreaWidth, 25, juce::Justification::centred, false);
                    paintKeyVolumeGraph (g);
                    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
                    g.setColour (juce::Colours::blue);
                    g.drawText ("1", contextAreaLeftEdge + 2, (int) topLineY, 36, 18, juce::Justification::centredLeft, false);
                    g.setColour (juce::Colours::red);
                    g.drawText ("0", contextAreaLeftEdge + 2, (int) (bounds.getHeight() - bounds.getHeight() / 8.0f) - 18, 36, 18, juce::Justification::centredLeft, false);
                }
                else if (activeAmpSubTab == 3)
                {
                    g.setColour (juce::Colours::black);
                    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
                    g.drawText ("STRENGTH OF EVENS", contextAreaLeftEdge, (int) topLineY, contextAreaWidth, 25, juce::Justification::centred, false);
                    paintEvenMorphGraph (g);
                    
                    int contentAreaTop = (int) (bounds.getHeight() / 16.0f) + 30;
                    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
                    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
                    int contentAreaCenterY = contentAreaTop + (contentAreaBottom - contentAreaTop) / 2;
                    g.setColour (juce::Colours::black);
                    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
                    g.drawText ("N TO N-SQUARED", contextAreaLeftEdge, contentAreaCenterY - (int)(45 * scale), contextAreaWidth, 25, juce::Justification::centred, false);
                }

                // 1 / 2 / 3 labels above slider snap positions
                {
                    auto sb    = ampSubTabSlider.getBounds();
                    int thumbR = 5;
                    int labelH = 11;
                    int labelW = 10;
                    int labelY = sb.getY() + sb.getHeight() / 2 - thumbR - labelH;
                    g.setColour (juce::Colours::black);
                    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f * (float)scale, juce::Font::plain));
                    g.drawText ("1",   sb.getX(),                       labelY, labelW, labelH, juce::Justification::centred, false);
                    g.drawText ("2",  sb.getCentreX()  - labelW / 2,   labelY, labelW, labelH, juce::Justification::centred, false);
                    g.drawText ("3", sb.getRight()    - labelW,        labelY, labelW, labelH, juce::Justification::centred, false);
                }
            }

    if (selectedKey == -1 && activeTab == 3)
            {
                paintBarGraph (g,
                    [this] (int fi) { return (double) audioProcessor.oscillators[fi].startPhaseDegrees; },
                    360.0, false);
                int phaseGraphTop    = (int) (bounds.getHeight() / 16.0f);
                int phaseGraphBottom = (int) (bounds.getHeight() - bounds.getHeight() / 8.0f);
                int phaseContextX    = (bounds.getWidth() * 3) / 4;
                g.setColour (juce::Colours::blue);
                g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain));
                g.drawText ("360", phaseContextX + 2, phaseGraphTop, 36, 18, juce::Justification::centredLeft, false);
                g.drawText ("1",   phaseContextX + 2, phaseGraphBottom - 18, 20, 18, juce::Justification::centredLeft, false);
            }

}

void SineLabAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    double scale = (double) bounds.getWidth() / 792.0;
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    float keyWidthExact = bounds.getWidth() / 88.0f;
    float keyboardTop = bounds.getHeight() - keyboardAreaHeight;
    float topLineY = bounds.getHeight() / 16.0f;
    float tabWidth = bounds.getWidth() / 9.0f;

    for (int i = 0; i < keyButtons.size(); ++i)
    {
        int xStart = (int) std::round (i * keyWidthExact);
        int xEnd   = (int) std::round ((i + 1) * keyWidthExact);
        keyButtons[i]->setBounds (xStart, (int) keyboardTop, xEnd - xStart, (int) keyboardAreaHeight);
    }

    int contentAreaTop = (int) (bounds.getHeight() / 16.0f) + 30;
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int contentAreaCenterY = contentAreaTop + (contentAreaBottom - contentAreaTop) / 2;
    int sliderHeight = contentAreaBottom - contentAreaTop;
    int sliderShift = 25;

    toggleViewport.setBounds (0, contentAreaTop - sliderShift, (bounds.getWidth() * 3/4), sliderHeight + 20);
    if (selectedKey != -1)
    {
        int count = audioProcessor.harmonicCounts[selectedKey];
        toggleContainer.setSize ((int)(88 * scale), (int)(count * 26 * scale));
        for (int h = 0; h < toggleButtons.size(); ++h)
        {
            int yPos = (int)((count - 1 - h) * 26 * scale);
            if (h < toggleButtons.size() && toggleButtons[h] != nullptr)
                toggleButtons[h]->setBounds (0, yPos, (int)(88 * scale), (int)(26 * scale));
            if (h < toggleLabels.size() && toggleLabels[h] != nullptr)
            {
                toggleLabels[h]->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), (float)(10.0f * scale), juce::Font::plain));
                toggleLabels[h]->setBounds ((int)(30 * scale), yPos, (int)(30 * scale), (int)(26 * scale));
            }
        }
    }

    int graphTop   = (int) (bounds.getHeight() / 16.0f);
    int graphBottom = contentAreaBottom;
    int graphRight  = (bounds.getWidth() * 3) / 4;
    int scrollBarThickness = toggleGraphViewport.getScrollBarThickness();
    toggleGraphViewport.setBounds (0, graphTop, graphRight + scrollBarThickness, graphBottom - graphTop);
    toggleGraphComponent.setSize (graphRight, (int)(727 * 10 * scale));

    globalToggleCheckbox.setBounds (bounds.getWidth() - (int)(30 * scale), contentAreaBottom - (int)(30 * scale), (int)(30 * scale), (int)(30 * scale));

    firstHarmonicToggleCheckbox.setBounds (bounds.getWidth() - (int)(30 * scale), contentAreaBottom - (int)(65 * scale), (int)(30 * scale), (int)(30 * scale));
    firstHarmonicLabel.setBounds (bounds.getWidth() - (int)(110 * scale), contentAreaBottom - (int)(65 * scale), (int)(75 * scale), (int)(30 * scale));

    evensToggleCheckbox.setBounds (bounds.getWidth() - (int)(30 * scale), contentAreaBottom - (int)(100 * scale), (int)(30 * scale), (int)(30 * scale));
    evensLabel.setBounds (bounds.getWidth() - (int)(110 * scale), contentAreaBottom - (int)(100 * scale), (int)(75 * scale), (int)(30 * scale));

    everyNToggleCheckbox.setBounds (bounds.getWidth() - (int)(145 * scale), contentAreaBottom - (int)(100 * scale), (int)(30 * scale), (int)(30 * scale));
    everyNTextBox.setBounds        (bounds.getWidth() - (int)(180 * scale), contentAreaBottom - (int)(100 * scale), (int)(30 * scale), (int)(30 * scale));

    primesToggleCheckbox.setBounds (bounds.getWidth() - (int)(145 * scale), contentAreaBottom - (int)(65 * scale), (int)(30 * scale), (int)(30 * scale));
    primesLabel.setBounds          (bounds.getWidth() - (int)(220 * scale), contentAreaBottom - (int)(65 * scale), (int)(75 * scale), (int)(30 * scale));

    toggleTabButton.setBounds (0, 0, (int) tabWidth, (int) topLineY);

    ampTabButton.setBounds ((int) tabWidth, 0, (int) tabWidth, (int) topLineY);
    globalAmpTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));

    normalizationCheckbox.setBounds (bounds.getWidth() - (int)(30 * scale), contentAreaBottom - (int)(65 * scale), (int)(30 * scale), (int)(30 * scale));
    taperCTCheckbox.setBounds  ((bounds.getWidth() * 3) / 4 + (bounds.getWidth() / 4) / 4 - (int)(50 * scale), contentAreaBottom - (int)(65 * scale),  (int)(100 * scale), (int)(30 * scale));
    taperACTCheckbox.setBounds ((bounds.getWidth() * 3) / 4 + (bounds.getWidth() / 4) / 4 - (int)(50 * scale), contentAreaBottom - (int)(95 * scale),  (int)(100 * scale), (int)(30 * scale));
    nToXTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(95 * scale), (int)(35 * scale), (int)(30 * scale));
    applyNToXButton.setBounds (bounds.getWidth() - (int)(40 * scale), contentAreaBottom - (int)(95 * scale), (int)(40 * scale), (int)(30 * scale));
    tuningTabButton.setBounds ((int) (2 * tabWidth), 0, (int) tabWidth, (int) topLineY);
        globalTuningTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));

    int contextAreaLeftEdge = (bounds.getWidth() * 3) / 4;
        int contextAreaWidth = bounds.getWidth() - contextAreaLeftEdge;
        int leftHalfCenterX = contextAreaLeftEdge + (contextAreaWidth / 4) - (int)(40 * scale);
        stretchA0TextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    
    int ampA0BoxLeft = contextAreaLeftEdge + contextAreaWidth / 2 - (int)(60 * scale);
    ampA0TextBox.setBounds (ampA0BoxLeft, contentAreaTop, (int)(60 * scale), (int)(30 * scale));

    int reducedTabButtonWidth = (30 * 3) / 4;
    int subTabMargin = 4;
    int masterBoxLeft = bounds.getWidth() - (int)(80 * scale);
    {
        int sliderW = reducedTabButtonWidth * 3 + subTabMargin * 2;
        int sliderX = masterBoxLeft - subTabMargin - sliderW;
        ampSubTabSlider.setBounds (sliderX, contentAreaBottom - 35, sliderW, 30);
    }
    {
        int dropdownW = (contextAreaWidth * 3) / 4;
        int dropdownH = (int)(24 * scale);
        int dropdownX = contextAreaLeftEdge + (contextAreaWidth - dropdownW) / 2;
        int dropdownY = (contentAreaTop + contentAreaBottom) / 2 - dropdownH / 2;
        int arrowW = dropdownH;  // square arrow buttons (scales automatically with dropdownH)
        presetBrowserComboBox.setBounds (dropdownX + (int)(arrowW * scale), dropdownY, dropdownW - (int)(arrowW * scale) * 2, dropdownH);
        prevPresetButton.setBounds (dropdownX, dropdownY, (int)(arrowW * scale), dropdownH);
        nextPresetButton.setBounds (dropdownX + dropdownW - (int)(arrowW * scale), dropdownY, (int)(arrowW * scale), dropdownH);
        savePresetButton.setBounds (dropdownX, dropdownY - dropdownH - (int)(4 * scale), (int)(50 * scale), dropdownH);
        deletePresetButton.setBounds (dropdownX + dropdownW - (int)(60 * scale), dropdownY - dropdownH - (int)(4 * scale), (int)(60 * scale), dropdownH);
    }
    {
        int sliderW = reducedTabButtonWidth * 2 + subTabMargin * 1;
        int sliderX = masterBoxLeft - subTabMargin - sliderW;
        tuningSubTabSlider.setBounds (sliderX, contentAreaBottom - 35, sliderW, 30);
    }

    int rightHalfCenterX = contextAreaLeftEdge + (contextAreaWidth * 3 / 4) - (int)(40 * scale);
    stretchC8TextBox.setBounds (rightHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    ampC8TextBox.setBounds (bounds.getWidth() - (int)(60 * scale), contentAreaTop, (int)(60 * scale), (int)(30 * scale));
    toggleA0TextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    toggleC8TextBox.setBounds (rightHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));

    int shapeRowLeft = leftHalfCenterX;
        int shapeRowRight = rightHalfCenterX + (int)(80 * scale) - (int)(30 * scale);
        int shapeRowSpan = shapeRowRight - shapeRowLeft;

    {
        int a0BoxLeft = contextAreaLeftEdge + contextAreaWidth / 2 - (int)(60 * scale);
        int upperRight = a0BoxLeft - (int)(5 * scale);
        int w = upperRight - shapeRowLeft;
        ampA0UpperTextBox.setBounds    (shapeRowLeft, contentAreaTop, w, (int)(30 * scale));
        evenMorphA0UpperTextBox.setBounds (shapeRowLeft, contentAreaTop, w, (int)(30 * scale));
        evenMorphA0CenterTextBox.setBounds (shapeRowLeft, contentAreaCenterY - (int)(15 * scale), w, (int)(30 * scale));
    }
    {
        int upperLeft = shapeRowLeft + (shapeRowSpan * 2) / 3;
        int w = (bounds.getWidth() - (int)(60 * scale)) - upperLeft - (int)(5 * scale);
        ampC8UpperTextBox.setBounds    (upperLeft, contentAreaTop, w, (int)(30 * scale));
        evenMorphC8UpperTextBox.setBounds (upperLeft, contentAreaTop, w, (int)(30 * scale));
        evenMorphC8CenterTextBox.setBounds (upperLeft, contentAreaCenterY - (int)(15 * scale), w, (int)(30 * scale));
    }

        linearShapeButton.setBounds (shapeRowLeft, contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        squaredShapeButton.setBounds (shapeRowLeft + shapeRowSpan / 3, contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        cubicShapeButton.setBounds (shapeRowLeft + (shapeRowSpan * 2) / 3, contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        absValueShapeButton.setBounds (shapeRowRight, contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        expShapeButton.setBounds (shapeRowLeft, contentAreaTop + (int)(70 * scale), (int)(30 * scale), (int)(30 * scale));
        expSteepnessTextBox.setBounds (shapeRowLeft + shapeRowSpan / 3, contentAreaTop + (int)(70 * scale), (int)(30 * scale), (int)(30 * scale));
        integralTextBox.setBounds (shapeRowLeft, contentAreaTop + (int)(105 * scale), (int)(60 * scale), (int)(30 * scale));
        integralLabel.setBounds (shapeRowLeft + (int)(65 * scale), contentAreaTop + (int)(105 * scale), (int)(80 * scale), (int)(30 * scale));

        evenMorphLinearCenterButton.setBounds   (shapeRowLeft, contentAreaCenterY + (int)(20 * scale), (int)(30 * scale), (int)(30 * scale));
        evenMorphSquaredCenterButton.setBounds  (shapeRowLeft + shapeRowSpan / 3, contentAreaCenterY + (int)(20 * scale), (int)(30 * scale), (int)(30 * scale));
        evenMorphCubicCenterButton.setBounds    (shapeRowLeft + (shapeRowSpan * 2) / 3, contentAreaCenterY + (int)(20 * scale), (int)(30 * scale), (int)(30 * scale));
        evenMorphAbsValueCenterButton.setBounds (shapeRowRight, contentAreaCenterY + (int)(20 * scale), (int)(30 * scale), (int)(30 * scale));
        evenMorphExpCenterButton.setBounds      (shapeRowLeft, contentAreaCenterY + (int)(55 * scale), (int)(30 * scale), (int)(30 * scale));
        evenMorphExpSteepnessCenterTextBox.setBounds (shapeRowLeft + shapeRowSpan / 3, contentAreaCenterY + (int)(55 * scale), (int)(30 * scale), (int)(30 * scale));

    phaseA0TextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    phaseC8TextBox.setBounds (rightHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    phaseEvensTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(105 * scale), (int)(80 * scale), (int)(30 * scale));
    phaseEvensLabel.setBounds (bounds.getWidth() - (int)(160 * scale), contentAreaBottom - (int)(105 * scale), (int)(75 * scale), (int)(30 * scale));
    phaseRandTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(70 * scale), (int)(80 * scale), (int)(30 * scale));
    phaseRandLabel.setBounds (bounds.getWidth() - (int)(160 * scale), contentAreaBottom - (int)(70 * scale), (int)(75 * scale), (int)(30 * scale));
    globalPhaseTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));
    globalPanTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));
    panWidthTextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    evenLeftOddRightButton.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(65 * scale), (int)(80 * scale), (int)(30 * scale));
    oddLeftEvenRightButton.setBounds (leftHalfCenterX, contentAreaBottom - (int)(65 * scale), (int)(80 * scale), (int)(30 * scale));
    twoLeftButton.setBounds (leftHalfCenterX, contentAreaBottom - (int)(95 * scale), (int)(80 * scale), (int)(30 * scale));
    alternateActiveButton.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(95 * scale), (int)(80 * scale), (int)(30 * scale));
    smartPanButton.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(125 * scale), (int)(80 * scale), (int)(30 * scale));

    globalAttackTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));
    attackRandTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(70 * scale), (int)(80 * scale), (int)(30 * scale));
    attackRandLabel.setBounds (bounds.getWidth() - (int)(160 * scale), contentAreaBottom - (int)(70 * scale), (int)(75 * scale), (int)(30 * scale));

    attackA0TextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
        attackC8TextBox.setBounds (rightHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    globalSustainTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));
    {
        int a0BoxLeft  = contextAreaLeftEdge + contextAreaWidth / 2 - (int)(60 * scale);
        int upperRight = a0BoxLeft - (int)(5 * scale);
        int upperW1    = upperRight - shapeRowLeft;
        sustainA0UpperTextBox.setBounds (shapeRowLeft, contentAreaTop, upperW1, (int)(30 * scale));

        sustainA0TextBox.setBounds (a0BoxLeft, contentAreaTop, (int)(60 * scale), (int)(30 * scale));

        int upperLeft = shapeRowLeft + (shapeRowSpan * 2) / 3;
        int upperW2   = (bounds.getWidth() - (int)(60 * scale)) - upperLeft - (int)(5 * scale);
        sustainC8UpperTextBox.setBounds (upperLeft, contentAreaTop, upperW2, (int)(30 * scale));

        sustainC8TextBox.setBounds (bounds.getWidth() - (int)(60 * scale), contentAreaTop, (int)(60 * scale), (int)(30 * scale));

        sustainLinearButton.setBounds   (shapeRowLeft,                        contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        sustainSquaredButton.setBounds  (shapeRowLeft + shapeRowSpan / 3,     contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        sustainCubicButton.setBounds    (shapeRowLeft + (shapeRowSpan * 2) / 3, contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        sustainAbsValueButton.setBounds (shapeRowRight,                       contentAreaTop + (int)(35 * scale), (int)(30 * scale), (int)(30 * scale));
        sustainExpButton.setBounds      (shapeRowLeft,                        contentAreaTop + (int)(70 * scale), (int)(30 * scale), (int)(30 * scale));
        sustainExpKTextBox.setBounds    (shapeRowLeft + shapeRowSpan / 3,     contentAreaTop + (int)(70 * scale), (int)(30 * scale), (int)(30 * scale));
    }
    globalDecayTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));

    decayA0TextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    decayC8TextBox.setBounds (rightHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    decayIIA0TextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    {
        int sliderW = reducedTabButtonWidth * 2 + subTabMargin * 1;
        int sliderX = masterBoxLeft - subTabMargin - sliderW;
        decaySubTabSlider.setBounds (sliderX, contentAreaBottom - 35, sliderW, 30);
    }
    releaseRandTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(70 * scale), (int)(80 * scale), (int)(30 * scale));
    releaseRandLabel.setBounds (bounds.getWidth() - (int)(160 * scale), contentAreaBottom - (int)(70 * scale), (int)(75 * scale), (int)(30 * scale));
    globalReleaseTextBox.setBounds (bounds.getWidth() - (int)(80 * scale), contentAreaBottom - (int)(35 * scale), (int)(80 * scale), (int)(30 * scale));
    releaseA0TextBox.setBounds (leftHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));
    releaseC8TextBox.setBounds (rightHalfCenterX, contentAreaTop, (int)(80 * scale), (int)(30 * scale));

    phaseTabButton.setBounds ((int) (3 * tabWidth), 0, (int) tabWidth, (int) topLineY);
    panTabButton.setBounds ((int) (4 * tabWidth), 0, (int) tabWidth, (int) topLineY);
    attackTabButton.setBounds ((int) (5 * tabWidth), 0, (int) tabWidth, (int) topLineY);
    decayTabButton.setBounds ((int) (6 * tabWidth), 0, (int) tabWidth, (int) topLineY);
    sustainTabButton.setBounds ((int) (7 * tabWidth), 0, (int) tabWidth, (int) topLineY);
    releaseTabButton.setBounds ((int) (8 * tabWidth), 0, (int) tabWidth, (int) topLineY);

    int verticalViewportWidth = (bounds.getWidth() * 3/4);
    int horizontalViewportTop = (int) topLineY;
            int horizontalViewportHeight = contentAreaBottom - horizontalViewportTop;



        amplitudeViewport.setBounds (0, contentAreaTop - sliderShift, verticalViewportWidth, sliderHeight + 20);

        keyVolumeSlider.setBounds (bounds.getWidth() - 40, contentAreaTop - sliderShift, 40, sliderHeight / 2);

        tuningViewport.setBounds (0, contentAreaTop - sliderShift, verticalViewportWidth, sliderHeight + 20);

        phaseViewport.setBounds (0, contentAreaTop - sliderShift, verticalViewportWidth, sliderHeight + 20);

    panViewport.setBounds (0, horizontalViewportTop, bounds.getWidth() * 3 / 4, horizontalViewportHeight);
            attackViewport.setBounds (0, horizontalViewportTop, bounds.getWidth() * 3 / 4, horizontalViewportHeight);
            decayViewport.setBounds (0, horizontalViewportTop, bounds.getWidth() * 3 / 4, horizontalViewportHeight);
            sustainViewport.setBounds (0, horizontalViewportTop, bounds.getWidth() * 3 / 4, horizontalViewportHeight);
            releaseViewport.setBounds (0, horizontalViewportTop, bounds.getWidth() * 3 / 4, horizontalViewportHeight);

    for (auto* child : getChildren())
    {
        if (auto* editor = dynamic_cast<juce::TextEditor*> (child))
        {
            auto font = juce::Font (juce::Font::getDefaultMonospacedFontName(), 14.0f * (float)scale, juce::Font::plain);
            editor->setFont (font);
            editor->applyFontToAllText (font);
        }
        else if (auto* label = dynamic_cast<juce::Label*> (child))
        {
            if (label->getParentComponent() == this)
            {
                label->setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.0f * (float)scale, juce::Font::plain));
            }
        }
    }
}

void SineLabAudioProcessorEditor::tabSelected (int tabIndex)
{
    activeTab = tabIndex;
    unfocusAllComponents();
    grabKeyboardFocus();

    if (selectedKey != -1)
        rebuildSlidersForActiveTab();

    if (activeTab == 0)
        repaint();

    auto& ap = audioProcessor;
    int osc0  = ap.keyStartIndex[0];
    int osc87 = ap.keyStartIndex[87];

    if (activeTab == 0)
    {
        expSteepnessTextBox.setText (juce::String (ap.lastAppliedExpKToggle), juce::dontSendNotification);
        toggleA0TextBox.setText (juce::String (ap.lastAppliedToggleA0), juce::dontSendNotification);
        toggleC8TextBox.setText (juce::String (ap.lastAppliedToggleC8), juce::dontSendNotification);
    }
    else if (activeTab == 1)
    {
        expSteepnessTextBox.setText (juce::String (activeAmpSubTab == 1 ? ap.lastAppliedExpKAmp
                                                 : activeAmpSubTab == 2 ? ap.lastAppliedExpKKeyVolume
                                                                        : ap.lastAppliedExpKEvenMorph), juce::dontSendNotification);
        if (activeAmpSubTab == 1)
        {
            ampA0TextBox.setText (juce::String (ap.lastAppliedAmpA0,  4), juce::dontSendNotification);
            ampC8TextBox.setText (juce::String (ap.lastAppliedAmpC8, 4), juce::dontSendNotification);
        }
        else if (activeAmpSubTab == 2)
        {
            ampA0TextBox.setText (juce::String (ap.lastAppliedKeyVolumeA0,  4), juce::dontSendNotification);
            ampC8TextBox.setText (juce::String (ap.lastAppliedKeyVolumeC8, 4), juce::dontSendNotification);
        }
        else
        {
            ampA0TextBox.setText (juce::String (ap.lastAppliedEvenMorphA0,  4), juce::dontSendNotification);
            ampC8TextBox.setText (juce::String (ap.lastAppliedEvenMorphC8, 4), juce::dontSendNotification);
            evenMorphA0UpperTextBox.setText (juce::String (ap.lastAppliedEvenMorphStartKey + 1), juce::dontSendNotification);
            evenMorphC8UpperTextBox.setText (juce::String (ap.lastAppliedEvenMorphEndKey   + 1), juce::dontSendNotification);
            evenMorphA0CenterTextBox.setText (juce::String (ap.lastAppliedNToNSquaredStartKey + 1), juce::dontSendNotification);
            evenMorphC8CenterTextBox.setText (juce::String (ap.lastAppliedNToNSquaredEndKey   + 1), juce::dontSendNotification);
            evenMorphExpSteepnessCenterTextBox.setText (juce::String (ap.lastAppliedExpKNToNSquared), juce::dontSendNotification);
        }
    }
    else if (activeTab == 2)
    {
        expSteepnessTextBox.setText (juce::String (activeTuningSubTab == 1 ? ap.lastAppliedExpKTuning1 : ap.lastAppliedExpKTuning2), juce::dontSendNotification);
        if (activeTuningSubTab == 1)
        {
            stretchA0TextBox.setText (juce::String (ap.lastAppliedStretchA0),  juce::dontSendNotification);
            stretchC8TextBox.setText (juce::String (ap.lastAppliedStretchC8), juce::dontSendNotification);
        }
        else
        {
            stretchA0TextBox.setText (juce::String (ap.lastAppliedInharmonicityA0,  4), juce::dontSendNotification);
            stretchC8TextBox.setText (juce::String (ap.lastAppliedInharmonicityC8, 4), juce::dontSendNotification);
        }
    }
    else if (activeTab == 3)
    {
        expSteepnessTextBox.setText (juce::String (ap.lastAppliedExpKPhase), juce::dontSendNotification);
        phaseA0TextBox.setText (juce::String (ap.lastAppliedPhaseA0),  juce::dontSendNotification);
        phaseC8TextBox.setText (juce::String (ap.lastAppliedPhaseC8), juce::dontSendNotification);
    }
    else if (activeTab == 4)
        panWidthTextBox.setText (juce::String (ap.lastAppliedPanWidth, 4), juce::dontSendNotification);
    else if (activeTab == 5)
    {
        expSteepnessTextBox.setText (juce::String (ap.lastAppliedExpKAttack), juce::dontSendNotification);
        attackA0TextBox.setText (juce::String (ap.lastAppliedAttackA0,  4), juce::dontSendNotification);
        attackC8TextBox.setText (juce::String (ap.lastAppliedAttackC8, 4), juce::dontSendNotification);
    }
    else if (activeTab == 6)
    {
        expSteepnessTextBox.setText (juce::String (activeDecaySubTab == 1 ? ap.lastAppliedExpKDecay1 : ap.lastAppliedExpKDecay2), juce::dontSendNotification);
        decayA0TextBox.setText  (juce::String (ap.lastAppliedDecayA0,  4), juce::dontSendNotification);
        decayC8TextBox.setText  (juce::String (ap.lastAppliedDecayC8, 4), juce::dontSendNotification);
        decayIIA0TextBox.setText (juce::String (ap.lastAppliedDecayIIThreshold, 4), juce::dontSendNotification);
    }
    else if (activeTab == 7)
    {
        sustainExpKTextBox.setText (juce::String (ap.lastAppliedExpKSustain), juce::dontSendNotification);
        sustainA0TextBox.setText (juce::String (ap.lastAppliedSustainA0,  4), juce::dontSendNotification);
        sustainC8TextBox.setText (juce::String (ap.lastAppliedSustainC8, 4), juce::dontSendNotification);
    }
    else if (activeTab == 8)
    {
        expSteepnessTextBox.setText (juce::String (ap.lastAppliedExpKRelease), juce::dontSendNotification);
        releaseA0TextBox.setText (juce::String (ap.lastAppliedReleaseA0,  4), juce::dontSendNotification);
        releaseC8TextBox.setText (juce::String (ap.lastAppliedReleaseC8, 4), juce::dontSendNotification);
    }

    wireShapeButtons();
    updateControlVisibility();

    if (activeTab == 0 && selectedKey == -1)
    {
        globalToggleCheckbox.setToggleState       (audioProcessor.globalToggleState,        juce::dontSendNotification);
        firstHarmonicToggleCheckbox.setToggleState (audioProcessor.firstHarmonicToggleState, juce::dontSendNotification);
        evensToggleCheckbox.setToggleState         (audioProcessor.evensToggleState,         juce::dontSendNotification);
        primesToggleCheckbox.setToggleState        (audioProcessor.primesToggleState,        juce::dontSendNotification);

    }

    repaint();
}

void SineLabAudioProcessorEditor::updateControlVisibility()
{
    toggleViewport.setVisible (selectedKey != -1 && activeTab == 0);
    toggleGraphViewport.setVisible (selectedKey == -1 && activeTab == 0);
    globalToggleCheckbox.setVisible (selectedKey == -1 && activeTab == 0);
    toggleA0TextBox.setVisible (selectedKey == -1 && activeTab == 0);
    toggleC8TextBox.setVisible (selectedKey == -1 && activeTab == 0);
    firstHarmonicToggleCheckbox.setVisible (selectedKey == -1 && activeTab == 0);
    firstHarmonicLabel.setVisible (selectedKey == -1 && activeTab == 0);
    firstHarmonicToggleCheckbox.setToggleState (audioProcessor.firstHarmonicToggleState, juce::dontSendNotification);
    globalToggleCheckbox.setToggleState (audioProcessor.globalToggleState, juce::dontSendNotification);
    evensToggleCheckbox.setVisible (selectedKey == -1 && activeTab == 0);
    evensLabel.setVisible (selectedKey == -1 && activeTab == 0);
    evensToggleCheckbox.setToggleState (audioProcessor.evensToggleState, juce::dontSendNotification);

    primesToggleCheckbox.setVisible (selectedKey == -1 && activeTab == 0);
    primesLabel.setVisible (selectedKey == -1 && activeTab == 0);
    primesToggleCheckbox.setToggleState (audioProcessor.primesToggleState, juce::dontSendNotification);

    everyNToggleCheckbox.setVisible (selectedKey == -1 && activeTab == 0);
    everyNTextBox.setVisible (selectedKey == -1 && activeTab == 0);
    everyNToggleCheckbox.setToggleState (audioProcessor.everyNToggleState, juce::dontSendNotification);
    everyNTextBox.setText (juce::String (audioProcessor.everyNValue), juce::dontSendNotification);

    amplitudeViewport.setVisible (selectedKey != -1 && activeTab == 1);
    keyVolumeSlider.setVisible (selectedKey != -1 && activeTab == 1);

    globalAmpTextBox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);
    ampA0TextBox.setVisible     (selectedKey == -1 && activeTab == 1 && activeAmpSubTab != 3);
    ampC8TextBox.setVisible     (selectedKey == -1 && activeTab == 1 && activeAmpSubTab != 3);
    
    normalizationCheckbox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);
    taperCTCheckbox.setVisible  (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);
    taperACTCheckbox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);
    taperCTCheckbox.setToggleState  (audioProcessor.taperCTEnabled,  juce::dontSendNotification);
    taperACTCheckbox.setToggleState (audioProcessor.taperACTEnabled, juce::dontSendNotification);
    ampA0UpperTextBox.setVisible      (selectedKey == -1 && activeTab == 1 && activeAmpSubTab != 3);
    ampC8UpperTextBox.setVisible      (selectedKey == -1 && activeTab == 1 && activeAmpSubTab != 3);
    evenMorphA0UpperTextBox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 3);
    evenMorphC8UpperTextBox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 3);
    evenMorphA0CenterTextBox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 3);
    evenMorphC8CenterTextBox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 3);
    
    bool evenMorphCenterControlsVisible = (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 3);
    evenMorphLinearCenterButton.setVisible   (evenMorphCenterControlsVisible);
    evenMorphSquaredCenterButton.setVisible  (evenMorphCenterControlsVisible);
    evenMorphCubicCenterButton.setVisible    (evenMorphCenterControlsVisible);
    evenMorphAbsValueCenterButton.setVisible (evenMorphCenterControlsVisible);
    evenMorphExpCenterButton.setVisible      (evenMorphCenterControlsVisible);
    evenMorphExpSteepnessCenterTextBox.setVisible (evenMorphCenterControlsVisible);

    ampSubTabSlider.setVisible (selectedKey == -1 && activeTab == 1);
    presetBrowserComboBox.setVisible (selectedKey == -1 && activeTab == 0);
    savePresetButton.setVisible      (selectedKey == -1 && activeTab == 0);
    deletePresetButton.setVisible    (selectedKey == -1 && activeTab == 0);
    prevPresetButton.setVisible      (selectedKey == -1 && activeTab == 0);
    nextPresetButton.setVisible      (selectedKey == -1 && activeTab == 0);
    applyNToXButton.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);
    nToXTextBox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);
    integralTextBox.setVisible (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);
    integralLabel.setVisible   (selectedKey == -1 && activeTab == 1 && activeAmpSubTab == 1);

    tuningViewport.setVisible (selectedKey != -1 && activeTab == 2);
        globalTuningTextBox.setVisible (selectedKey == -1 && activeTab == 2);

    stretchA0TextBox.setVisible (selectedKey == -1 && activeTab == 2);
    stretchC8TextBox.setVisible (selectedKey == -1 && activeTab == 2);
    bool shapeButtonsVisible = (selectedKey == -1 && (activeTab == 0 || activeTab == 1 || activeTab == 2 || activeTab == 3 || activeTab == 4 || activeTab == 5 || activeTab == 6 || activeTab == 8));
    linearShapeButton.setVisible  (shapeButtonsVisible && activeTab != 4);
    cubicShapeButton.setVisible   (shapeButtonsVisible && activeTab != 4);
    squaredShapeButton.setVisible (shapeButtonsVisible && activeTab != 0 && activeTab != 4 && !(activeTab == 6 && activeDecaySubTab == 2));
    absValueShapeButton.setVisible(shapeButtonsVisible && activeTab != 0 && activeTab != 4 && !(activeTab == 6 && activeDecaySubTab == 2));
    expShapeButton.setVisible     (shapeButtonsVisible && activeTab != 4);
    expSteepnessTextBox.setVisible(shapeButtonsVisible && activeTab != 4);
    tuningSubTabSlider.setVisible (selectedKey == -1 && activeTab == 2);

    phaseViewport.setVisible (selectedKey != -1 && activeTab == 3);
    bool phaseGlobalVisible = (selectedKey == -1 && activeTab == 3);
    globalPhaseTextBox.setVisible (phaseGlobalVisible);
    phaseA0TextBox.setVisible (phaseGlobalVisible);
    phaseC8TextBox.setVisible (phaseGlobalVisible);
    phaseEvensTextBox.setVisible (phaseGlobalVisible);
    phaseEvensLabel.setVisible (phaseGlobalVisible);
    phaseRandTextBox.setVisible (phaseGlobalVisible);
    phaseRandLabel.setVisible (phaseGlobalVisible);

    panViewport.setVisible (selectedKey != -1 && activeTab == 4);
    
    bool panGlobalVisible = (selectedKey == -1 && activeTab == 4);
    globalPanTextBox.setVisible (panGlobalVisible);
    panWidthTextBox.setVisible (panGlobalVisible);
    evenLeftOddRightButton.setVisible (panGlobalVisible);
    oddLeftEvenRightButton.setVisible (panGlobalVisible);
    twoLeftButton.setVisible (panGlobalVisible);
    alternateActiveButton.setVisible (panGlobalVisible);
    smartPanButton.setVisible (panGlobalVisible);

    attackViewport.setVisible (selectedKey != -1 && activeTab == 5);

    bool attackGlobalVisible = (selectedKey == -1 && activeTab == 5);
    globalAttackTextBox.setVisible (attackGlobalVisible);
    attackA0TextBox.setVisible (attackGlobalVisible);
    attackC8TextBox.setVisible (attackGlobalVisible);
    attackRandTextBox.setVisible (attackGlobalVisible);
    attackRandLabel.setVisible (attackGlobalVisible);

    decayViewport.setVisible (selectedKey != -1 && activeTab == 6);
    
    globalDecayTextBox.setVisible (selectedKey == -1 && activeTab == 6);

    decayA0TextBox.setVisible (selectedKey == -1 && activeTab == 6 && activeDecaySubTab == 1);
    decayC8TextBox.setVisible (selectedKey == -1 && activeTab == 6 && activeDecaySubTab == 1);
    decayIIA0TextBox.setVisible (selectedKey == -1 && activeTab == 6 && activeDecaySubTab == 2);
    decaySubTabSlider.setVisible (selectedKey == -1 && activeTab == 6);
    sustainViewport.setVisible (selectedKey != -1 && activeTab == 7);
        globalSustainTextBox.setVisible (selectedKey == -1 && activeTab == 7);
    {
        bool sv = (selectedKey == -1 && activeTab == 7);
        sustainA0UpperTextBox.setVisible (sv);
        sustainA0TextBox.setVisible      (sv);
        sustainC8UpperTextBox.setVisible (sv);
        sustainC8TextBox.setVisible      (sv);
        sustainLinearButton.setVisible   (sv);
        sustainSquaredButton.setVisible  (sv);
        sustainCubicButton.setVisible    (sv);
        sustainAbsValueButton.setVisible (sv);
        sustainExpButton.setVisible      (sv);
        sustainExpKTextBox.setVisible    (sv);
    }
    
    

    releaseViewport.setVisible (selectedKey != -1 && activeTab == 8);
    bool releaseGlobalVisible = (selectedKey == -1 && activeTab == 8);
    globalReleaseTextBox.setVisible (releaseGlobalVisible);
    releaseA0TextBox.setVisible (releaseGlobalVisible);
    releaseC8TextBox.setVisible (releaseGlobalVisible);
    releaseRandTextBox.setVisible (releaseGlobalVisible);
    releaseRandLabel.setVisible (releaseGlobalVisible);
    
    
}

void SineLabAudioProcessorEditor::toggleButtonChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        audioProcessor.oscillators[index].manuallyMuted = ! toggleButtons[harmonicIndex]->getToggleState();
        recalculateKeyVolume (selectedKey);
        repaint();
    }
}

void SineLabAudioProcessorEditor::rebuildToggleButtons()
{
    toggleButtons.clear();
    toggleLabels.clear();

    if (selectedKey == -1)
        return;

    double scale = (double) getWidth() / 792.0;
    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];

    for (int h = 0; h < count; ++h)
    {
        auto* button = new juce::ToggleButton();
        button->setLookAndFeel (&scalingToggleLookAndFeel);
        button->setWantsKeyboardFocus (false);
        button->setButtonText ("");
        button->setColour (juce::ToggleButton::textColourId, juce::Colours::black);

        int harmonicIndex = h;
        button->onClick = [this, harmonicIndex] { toggleButtonChanged (harmonicIndex); };

        auto& thisOsc = audioProcessor.oscillators[startIndex + h];
        bool isOn = thisOsc.isAudible();
        button->setToggleState (isOn, juce::dontSendNotification);

        bool outOfRange  = (thisOsc.tuningCents >= thisOsc.audibleMaxCents || thisOsc.tuningCents <= thisOsc.maxDownwardCents);
        bool ampOff      = (thisOsc.amplitude <= 1e-5);
        bool decayOff    = (thisOsc.decayTime == 0.0f && thisOsc.sustainLevel == 0.0f);
        bool isSilent    = outOfRange || ampOff || decayOff || thisOsc.aboveCeiling || thisOsc.manuallyMuted;
        auto indicatorColour = isSilent ? juce::Colours::lightgrey : juce::Colours::black;
        button->setColour (juce::ToggleButton::tickColourId,         indicatorColour);
        button->setColour (juce::ToggleButton::tickDisabledColourId, indicatorColour);

        toggleContainer.addAndMakeVisible (button);
        toggleButtons.add (button);

        int yPos = (int)((count - 1 - h) * 26 * scale);
        button->setBounds (0, yPos, (int)(88 * scale), (int)(26 * scale));

        auto* label = new juce::Label();
        label->setText (juce::String (h + 1), juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centredLeft);
        label->setColour (juce::Label::textColourId, juce::Colours::black);
        label->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), (float)(10.0f * scale), juce::Font::plain));
        label->setBounds ((int)(30 * scale), yPos, (int)(30 * scale), (int)(26 * scale));

        toggleContainer.addAndMakeVisible (label);
        toggleLabels.add (label);
    }

    toggleContainer.setSize ((int)(88 * scale), (int)(count * 26 * scale));
    toggleViewport.setViewPosition (0, toggleContainer.getHeight());
}



void SineLabAudioProcessorEditor::globalToggleChanged()
{
    bool turnOn = globalToggleCheckbox.getToggleState();
    audioProcessor.globalToggleState = turnOn;

    for (auto& osc : audioProcessor.oscillators)
        osc.manuallyMuted = ! turnOn;

    audioProcessor.updateActiveRanks();
    recalculateAllKeyVolumes();
    repaint();

    // Refresh the UI lists to match the new engine state
    if (selectedKey != -1 && activeTab == 0)
        rebuildToggleButtons();
}

void SineLabAudioProcessorEditor::firstHarmonicToggleChanged()
{
    bool turnOn = firstHarmonicToggleCheckbox.getToggleState();
    audioProcessor.firstHarmonicToggleState = turnOn;

    for (int key = 0; key < 88; ++key)
    {
        int index = audioProcessor.keyStartIndex[key];
        audioProcessor.oscillators[index].manuallyMuted = ! turnOn;
    }
    audioProcessor.updateActiveRanks();
    recalculateAllKeyVolumes();
    repaint();

    // Refresh the UI lists to match the new engine state
    if (selectedKey != -1 && activeTab == 0)
        rebuildToggleButtons();
}

void SineLabAudioProcessorEditor::evensToggleChanged()
{
    bool turnOn = evensToggleCheckbox.getToggleState();
    audioProcessor.evensToggleState = turnOn;

    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = 1; h < count; h += 2)
            audioProcessor.oscillators[startIndex + h].manuallyMuted = ! turnOn;
    }

    audioProcessor.updateActiveRanks();
    recalculateAllKeyVolumes();
    repaint();

    if (selectedKey != -1 && activeTab == 0)
        rebuildToggleButtons();
}

static bool isPrime (int n)
{
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int i = 3; i * i <= n; i += 2)
        if (n % i == 0) return false;
    return true;
}

void SineLabAudioProcessorEditor::primesToggleChanged()
{
    bool turnOn = primesToggleCheckbox.getToggleState();
    audioProcessor.primesToggleState = turnOn;

    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = 0; h < count; ++h)
        {
            int n = h + 1;
            if (isPrime (n))
                audioProcessor.oscillators[startIndex + h].manuallyMuted = ! turnOn;
        }
    }

    audioProcessor.updateActiveRanks();
    recalculateAllKeyVolumes();
    repaint();

    if (selectedKey != -1 && activeTab == 0)
        rebuildToggleButtons();
}

void SineLabAudioProcessorEditor::everyNToggleChanged()
{
    bool turnOn = everyNToggleCheckbox.getToggleState();
    audioProcessor.everyNToggleState = turnOn;

    int n = juce::jlimit (3, 99, audioProcessor.everyNValue);

    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = n - 1; h < count; h += n)
            audioProcessor.oscillators[startIndex + h].manuallyMuted = ! turnOn;
    }

    audioProcessor.updateActiveRanks();
    recalculateAllKeyVolumes();
    repaint();

    if (selectedKey != -1 && activeTab == 0)
        rebuildToggleButtons();
}

static float computeTaperFactor (const SineLabAudioProcessor& p, int oscIndex)
{
    const auto& osc = p.oscillators[oscIndex];
    float taper = 1.0f;
    if (p.taperCTEnabled)
    {
        int count = p.harmonicCounts[osc.ownerKey];
        int h = osc.harmonicNumber - 1;
        if (count > 0)
            taper *= (float)(count - h) / (float)count;
    }
    if (p.taperACTEnabled && osc.activeCount > 0)
        taper *= (float)(osc.activeCount - osc.activeRank) / (float)osc.activeCount;
    return taper;
}

void SineLabAudioProcessorEditor::amplitudeSliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        float taper = computeTaperFactor (audioProcessor, index);
        double sliderVal = amplitudeSliders[harmonicIndex]->getValue();
        audioProcessor.oscillators[index].amplitude = (taper > 0.0f) ? sliderVal / taper : sliderVal;
        recalculateKeyVolume (selectedKey);
        repaintGraphArea();
    }
}

void SineLabAudioProcessorEditor::rebuildAmplitudeSliders()
{
    if (selectedKey != -1 && selectedKey == amplitudeBuiltKey
        && amplitudeSliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < amplitudeSliders.size(); ++h)
        {
            float taper = computeTaperFactor (audioProcessor, startIndex + h);
            amplitudeSliders[h]->setValue (audioProcessor.oscillators[startIndex + h].amplitude * taper, juce::dontSendNotification);
        }
        return;
    }

    amplitudeBuiltKey = -1;
    amplitudeSliders.clear();
    amplitudeLabels.clear();

    if (selectedKey == -1)
        return;

    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];

    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f) + 30;
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int sliderHeight = contentAreaBottom - contentAreaTop;

    for (int h = 0; h < count; ++h)
    {
        int oscIndex = startIndex + h;
        float taper = computeTaperFactor (audioProcessor, oscIndex);
        double displayVal = audioProcessor.oscillators[oscIndex].amplitude * taper;
        // interval 0 = no snapping, so the box holds the TRUE stored amplitude
        auto* slider = setupHarmonicSlider (amplitudeContainer, amplitudeLabels, 0.0, 1.0, 0.0, h, displayVal, [this] (int harmonicIndex) { amplitudeSliderChanged (harmonicIndex); });
        slider->setNumDecimalPlacesToDisplay (8);
        amplitudeSliders.add (slider);
    }

    amplitudeContainer.setSize (count * 40, sliderHeight + 12);
    amplitudeBuiltKey = selectedKey;
}

void SineLabAudioProcessorEditor::keyVolumeChanged()
{
    if (selectedKey != -1)
        audioProcessor.keyVolume[selectedKey] = keyVolumeSlider.getValue();
}

void SineLabAudioProcessorEditor::normalizationToggled()
{
    audioProcessor.normalizationEnabled = normalizationCheckbox.getToggleState();

    if (audioProcessor.normalizationEnabled)
    {
        for (int key = 0; key < 88; ++key)
            recalculateKeyVolume (key);

        if (selectedKey != -1)
            keyVolumeSlider.setValue (audioProcessor.keyVolume[selectedKey], juce::dontSendNotification);
    }
}

void SineLabAudioProcessorEditor::taperCTToggled()
{
    audioProcessor.taperCTEnabled = taperCTCheckbox.getToggleState();
    if (audioProcessor.taperCTEnabled)
    {
        audioProcessor.taperACTEnabled = false;
        taperACTCheckbox.setToggleState (false, juce::dontSendNotification);
    }
    audioProcessor.updateActiveRanks();
    if (selectedKey != -1) rebuildAmplitudeSliders();
    repaint();
}

void SineLabAudioProcessorEditor::taperACTToggled()
{
    audioProcessor.taperACTEnabled = taperACTCheckbox.getToggleState();
    if (audioProcessor.taperACTEnabled)
    {
        audioProcessor.taperCTEnabled = false;
        taperCTCheckbox.setToggleState (false, juce::dontSendNotification);
    }
    audioProcessor.updateActiveRanks();
    if (selectedKey != -1) rebuildAmplitudeSliders();
    repaint();
}






void SineLabAudioProcessorEditor::applyNToXClicked()
{
    double xVal = nToXTextBox.getText().getDoubleValue();
    xVal = juce::jlimit (0.01, 3.00, xVal);
    nToXTextBox.setText (juce::String (xVal, 2), juce::dontSendNotification);

    for (auto& osc : audioProcessor.oscillators)
        osc.amplitude = 1.0 / std::pow ((double)osc.harmonicNumber, xVal);

    for (int key = 0; key < 88; ++key)
        recalculateKeyVolume (key);

    if (selectedKey != -1)
        rebuildAmplitudeSliders();

    repaintGraphArea();
}





void SineLabAudioProcessorEditor::recalculateKeyVolume (int key)
{
    if (! audioProcessor.normalizationEnabled)
        return;

    double simSr = audioProcessor.getSampleRate();
    if (simSr <= 0.0) simSr = 44100.0;
    double fundFreq = 440.0 * std::pow (2.0, (key + 21 - 69) / 12.0);
    int simSamples = (int)std::ceil (2.0 * simSr / fundFreq);
    simSamples = juce::jlimit (1000, 8000, simSamples);

    int startIndex = audioProcessor.keyStartIndex[key];
    int count = audioProcessor.harmonicCounts[key];

    struct ActiveOsc {
        double rCos, rSin;
        double dCos, dSin;
        double ampLeft, ampRight;
    };
    std::vector<ActiveOsc> activeOscs;
    activeOscs.reserve (count);

    for (int h = 0; h < count; ++h)
    {
        auto& osc = audioProcessor.oscillators[startIndex + h];
        if (osc.isAudible())
        {
            double oscFreq = osc.harmonicNumber * fundFreq * std::pow (2.0, osc.tuningCents / 1200.0);
            double angleDelta = (oscFreq / simSr) * 2.0 * juce::MathConstants<double>::pi;
            
            double dCos = std::cos (angleDelta);
            double dSin = std::sin (angleDelta);
            double rCos = std::cos (osc.startPhase);
            double rSin = std::sin (osc.startPhase);
            
            double panAngle = (osc.pan + 1.0) * (juce::MathConstants<double>::pi / 4.0);
            double leftGain = std::cos (panAngle);
            double rightGain = std::sin (panAngle);
            float taper = computeTaperFactor (audioProcessor, startIndex + h);
            double effectiveAmp = osc.amplitude * taper;
            
            activeOscs.push_back ({ rCos, rSin, dCos, dSin, effectiveAmp * leftGain, effectiveAmp * rightGain });
        }
    }

    if (activeOscs.empty())
    {
        audioProcessor.keyVolume[key] = 1.0;
        if (key == selectedKey)
            keyVolumeSlider.setValue (1.0, juce::dontSendNotification);
        return;
    }

    double maxVal = 0.0;
    for (int s = 0; s < simSamples; ++s)
    {
        double sampleLeft = 0.0;
        double sampleRight = 0.0;
        for (auto& osc : activeOscs)
        {
            sampleLeft += osc.ampLeft * osc.rSin;
            sampleRight += osc.ampRight * osc.rSin;
            
            // Perform rotation exactly like the DSP engine
            double nc = osc.rCos * osc.dCos - osc.rSin * osc.dSin;
            double ns = osc.rSin * osc.dCos + osc.rCos * osc.dSin;
            osc.rCos = nc;
            osc.rSin = ns;
        }
        double absVal = std::max (std::abs (sampleLeft), std::abs (sampleRight));
        if (absVal > maxVal)
            maxVal = absVal;
    }

    double targetPeak = 0.5; // -6.0 dB headroom (exactly 1/2)
    double newVolume = (maxVal > 0.0) ? (targetPeak / maxVal) : 1.0;
    newVolume = std::min (newVolume, 1.0);
    audioProcessor.keyVolume[key] = newVolume;

    if (key == selectedKey)
        keyVolumeSlider.setValue (newVolume, juce::dontSendNotification);
}


void SineLabAudioProcessorEditor::recalculateAllKeyVolumes()
{
    for (int key = 0; key < 88; ++key)
        recalculateKeyVolume (key);
}

void SineLabAudioProcessorEditor::reapplyDecayShapeForKey (int key)
{
    int startIndex = audioProcessor.keyStartIndex[key];
    int count = audioProcessor.harmonicCounts[key];
    double fundDecay = audioProcessor.oscillators[startIndex].decayTime;
    for (int h = 1; h < count; ++h)
        audioProcessor.oscillators[startIndex + h].decayTime = fundDecay * audioProcessor.oscillators[startIndex + h].decayShapeRatio;
}

void SineLabAudioProcessorEditor::globalAmpApplied()
{
    double value = juce::jlimit (0.0, 1.0, globalAmpTextBox.getText().getDoubleValue());
    globalAmpTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.globalAmpValue = value;
    for (auto& osc : audioProcessor.oscillators)
        osc.amplitude = value;
    recalculateAllKeyVolumes();
    repaintGraphArea();
    globalAmpTextBox.giveAwayKeyboardFocus();
}

static void applyPulseWaveToKey (SineLabAudioProcessor& ap, int key, double dutyCycle, double alpha)
{
    ap.keyDutyCycle[key] = dutyCycle;

    int startIndex = ap.keyStartIndex[key];
    int count = ap.harmonicCounts[key];

    if (dutyCycle == 0.0)
    {
        for (int h = 0; h < count; ++h)
            ap.oscillators[startIndex + h].amplitude = 0.0;
        return;
    }

    double maxAmp = 0.0;
    for (int h = 0; h < count; ++h)
    {
        int n = ap.oscillators[startIndex + h].harmonicNumber;
        double raw = std::abs ((4.0 / (n * juce::MathConstants<double>::pi))
                               * std::sin (n * juce::MathConstants<double>::pi * dutyCycle));
        if (alpha > 0.0)
            raw /= std::pow ((double) n, alpha);

        ap.oscillators[startIndex + h].amplitude = raw;
        maxAmp = std::max (maxAmp, raw);
    }

    if (maxAmp > 0.0)
        for (int h = 0; h < count; ++h)
            ap.oscillators[startIndex + h].amplitude /= maxAmp;
}

void SineLabAudioProcessorEditor::integralApplied()
{
    double alpha = juce::jlimit (0.0, 1.0, integralTextBox.getText().getDoubleValue());
    integralTextBox.setText (juce::String (alpha, 2), juce::dontSendNotification);

    audioProcessor.lastAppliedIntegralValue = alpha;

    for (int key = 0; key < 88; ++key)
    {
        double dc = audioProcessor.keyDutyCycle[key];
        int startIndex = audioProcessor.keyStartIndex[key];
        int count      = audioProcessor.harmonicCounts[key];

        if (dc == 0.0 || count <= 0)
            continue;

        // Recompute the un-integrated pure pulse wave amplitudes for this key
        std::vector<double> rawAmps (count);
        double maxRaw = 0.0;
        for (int h = 0; h < count; ++h)
        {
            int n = audioProcessor.oscillators[startIndex + h].harmonicNumber;
            double raw = std::abs ((4.0 / (n * juce::MathConstants<double>::pi))
                                   * std::sin (n * juce::MathConstants<double>::pi * dc));
            rawAmps[h] = raw;
            maxRaw = std::max (maxRaw, raw);
        }
        if (maxRaw <= 0.0)
            continue;
        // Normalize to [0,1] the same way applyPulseWaveToKey does
        for (auto& a : rawAmps)
            a /= maxRaw;

        // Apply fractional integral: scale harmonic n by 1/n^alpha
        double maxAmp = 0.0;
        for (int h = 0; h < count; ++h)
        {
            int n = audioProcessor.oscillators[startIndex + h].harmonicNumber;
            double scaled = rawAmps[h] / std::pow ((double) n, alpha);
            audioProcessor.oscillators[startIndex + h].amplitude = scaled;
            maxAmp = std::max (maxAmp, scaled);
        }
        // Re-normalize so the loudest harmonic stays at 1 (preserves existing volume behaviour)
        if (maxAmp > 0.0)
            for (int h = 0; h < count; ++h)
                audioProcessor.oscillators[startIndex + h].amplitude /= maxAmp;

        recalculateKeyVolume (key);
    }

    repaintGraphArea();
    if (selectedKey != -1)
        rebuildAmplitudeSliders();
    integralTextBox.giveAwayKeyboardFocus();
}


void SineLabAudioProcessorEditor::ampA0Applied()
{
    if (activeAmpSubTab == 1)
    {
        double value = juce::jlimit (0.0, 0.5, ampA0TextBox.getText().getDoubleValue());
        ampA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
        int startKey = audioProcessor.lastAppliedAmpStartKey;
        int endKey   = audioProcessor.lastAppliedAmpEndKey;
        double rightVal = juce::jlimit (0.0, 0.5, ampC8TextBox.getText().getDoubleValue());
        double alpha = juce::jlimit (0.0, 1.0, integralTextBox.getText().getDoubleValue());
        for (int key = 0; key < 88; ++key)
        {
            if (key <= startKey)
            {
                applyPulseWaveToKey (audioProcessor, key, value, alpha);
                recalculateKeyVolume (key);
            }
            else if (key >= endKey)
            {
                applyPulseWaveToKey (audioProcessor, key, rightVal, alpha);
                recalculateKeyVolume (key);
            }
        }
        repaint();
        if (selectedKey != -1 && (selectedKey <= startKey || selectedKey >= endKey)) rebuildAmplitudeSliders();
        audioProcessor.lastAppliedAmpA0 = value;
    }
    else if (activeAmpSubTab == 2)
    {
        double value = juce::jlimit (0.0, 1.0, ampA0TextBox.getText().getDoubleValue());
        ampA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
        int keyIndex = audioProcessor.lastAppliedKeyVolumeStartKey;
        audioProcessor.keyVolume[keyIndex] = value;
        if (selectedKey == keyIndex)
            keyVolumeSlider.setValue (value, juce::dontSendNotification);
        repaint();
        audioProcessor.lastAppliedKeyVolumeA0 = value;
    }
    else if (activeAmpSubTab == 3)
    {
        double value = juce::jlimit (0.0, 1.0, ampA0TextBox.getText().getDoubleValue());
        ampA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
        int startKey = audioProcessor.lastAppliedEvenMorphStartKey;
        for (int key = 0; key <= startKey; ++key)
        {
            audioProcessor.evenMorphStrength[key] = value;
            int base = audioProcessor.keyStartIndex[key];
            int count = audioProcessor.harmonicCounts[key];
            for (int h = 0; h < count; ++h)
            {
                int n = audioProcessor.oscillators[base + h].harmonicNumber;
                audioProcessor.oscillators[base + h].amplitude = (n % 2 == 0) ? value / n : 1.0 / n;
            }
            recalculateKeyVolume (key);
        }
        repaint();
        if (selectedKey >= 0 && selectedKey <= startKey) rebuildAmplitudeSliders();
        audioProcessor.lastAppliedEvenMorphA0 = value;
    }
    ampA0TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::ampC8Applied()
{
    if (activeAmpSubTab == 1)
    {
        double value = juce::jlimit (0.0, 0.5, ampC8TextBox.getText().getDoubleValue());
        ampC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
        int startKey = audioProcessor.lastAppliedAmpStartKey;
        int endKey   = audioProcessor.lastAppliedAmpEndKey;
        double leftVal = juce::jlimit (0.0, 0.5, ampA0TextBox.getText().getDoubleValue());
        double alpha = juce::jlimit (0.0, 1.0, integralTextBox.getText().getDoubleValue());
        for (int key = 0; key < 88; ++key)
        {
            if (key <= startKey)
            {
                applyPulseWaveToKey (audioProcessor, key, leftVal, alpha);
                recalculateKeyVolume (key);
            }
            else if (key >= endKey)
            {
                applyPulseWaveToKey (audioProcessor, key, value, alpha);
                recalculateKeyVolume (key);
            }
        }
        repaint();
        if (selectedKey != -1 && (selectedKey <= startKey || selectedKey >= endKey)) rebuildAmplitudeSliders();
        audioProcessor.lastAppliedAmpC8 = value;
    }

    else if (activeAmpSubTab == 2)
    {
        double value = juce::jlimit (0.0, 1.0, ampC8TextBox.getText().getDoubleValue());
        ampC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
        int keyIndex = audioProcessor.lastAppliedKeyVolumeEndKey;
        audioProcessor.keyVolume[keyIndex] = value;
        if (selectedKey == keyIndex)
            keyVolumeSlider.setValue (value, juce::dontSendNotification);
        repaint();
        audioProcessor.lastAppliedKeyVolumeC8 = value;
    }
    else if (activeAmpSubTab == 3)
    {
        double value = juce::jlimit (0.0, 1.0, ampC8TextBox.getText().getDoubleValue());
        ampC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
        int endKey = audioProcessor.lastAppliedEvenMorphEndKey;
        for (int key = endKey; key < 88; ++key)
        {
            audioProcessor.evenMorphStrength[key] = value;
            int base = audioProcessor.keyStartIndex[key];
            int count = audioProcessor.harmonicCounts[key];
            for (int h = 0; h < count; ++h)
            {
                int n = audioProcessor.oscillators[base + h].harmonicNumber;
                audioProcessor.oscillators[base + h].amplitude = (n % 2 == 0) ? value / n : 1.0 / n;
            }
            recalculateKeyVolume (key);
        }
        repaint();
        if (selectedKey >= endKey) rebuildAmplitudeSliders();
        audioProcessor.lastAppliedEvenMorphC8 = value;
    }
    ampC8TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::globalTuningApplied()
{
    int value = juce::jlimit (-3600, 3600, globalTuningTextBox.getText().getIntValue());
    globalTuningTextBox.setText (juce::String (value), juce::dontSendNotification);
    audioProcessor.globalTuningValue = value;

    for (auto& osc : audioProcessor.oscillators)
    {
        osc.stretchCents = value;
        osc.recombineTuningCents();
        osc.needsFrequencyUpdate = true;
    }

    repaint();
    globalTuningTextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::stretchA0Applied()
{
    if (activeTuningSubTab == 2)
    {
        
        double value = stretchA0TextBox.getText().getDoubleValue();
                value = juce::jlimit (-0.0015, 0.0015, value);
                stretchA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);

        double B = value;
        int key = 0;
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        audioProcessor.keyInharmonicityB[key] = B;

        for (int h = 0; h < count; ++h)
        {
            int harmonicNumber = h + 1;
            double magnitudeTerm = std::max (0.0001, 1.0 + B * harmonicNumber * harmonicNumber);
            double cents = 1200.0 * std::log2 (std::sqrt (magnitudeTerm));

            audioProcessor.oscillators[startIndex + h].inharmonicityCents = (int) cents;
            audioProcessor.oscillators[startIndex + h].recombineTuningCents();
            audioProcessor.oscillators[startIndex + h].needsFrequencyUpdate = true;
        }

        recalculateKeyVolume (key);
        repaint();

        if (selectedKey == key)
            rebuildTuningSliders();

        audioProcessor.lastAppliedInharmonicityA0 = B;
        stretchA0TextBox.giveAwayKeyboardFocus();
        return;
    }

    int value = (int) stretchA0TextBox.getText().getDoubleValue();
    value = juce::jlimit (-50, 50, value);
    stretchA0TextBox.setText (juce::String (value), juce::dontSendNotification);

    int key = 0;
    int startIndex = audioProcessor.keyStartIndex[key];
    int count = audioProcessor.harmonicCounts[key];

    for (int h = 0; h < count; ++h)
    {
        audioProcessor.oscillators[startIndex + h].stretchCents = value;
        audioProcessor.oscillators[startIndex + h].recombineTuningCents();
        audioProcessor.oscillators[startIndex + h].needsFrequencyUpdate = true;
    }

    repaint();

    if (selectedKey == key)
        rebuildTuningSliders();

    audioProcessor.lastAppliedStretchA0 = value;
    stretchA0TextBox.giveAwayKeyboardFocus();
}


void SineLabAudioProcessorEditor::stretchC8Applied()
{
    if (activeTuningSubTab == 2)
    {
        double value = stretchC8TextBox.getText().getDoubleValue();
                value = juce::jlimit (-0.0015, 0.0015, value);
                stretchC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);

        double B = value;
        int key = 87;
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        audioProcessor.keyInharmonicityB[key] = B;

        for (int h = 0; h < count; ++h)
        {
            int harmonicNumber = h + 1;
            double magnitudeTerm = std::max (0.0001, 1.0 + B * harmonicNumber * harmonicNumber);
            double cents = 1200.0 * std::log2 (std::sqrt (magnitudeTerm));

            audioProcessor.oscillators[startIndex + h].inharmonicityCents = (int) cents;
            audioProcessor.oscillators[startIndex + h].recombineTuningCents();
            audioProcessor.oscillators[startIndex + h].needsFrequencyUpdate = true;
        }

        recalculateKeyVolume (key);
        repaint();

        if (selectedKey == key)
            rebuildTuningSliders();

        audioProcessor.lastAppliedInharmonicityC8 = B;
        stretchC8TextBox.giveAwayKeyboardFocus();
        return;
    }

    int value = (int) stretchC8TextBox.getText().getDoubleValue();
    value = juce::jlimit (-50, 50, value);
    stretchC8TextBox.setText (juce::String (value), juce::dontSendNotification);

    int key = 87;
    int startIndex = audioProcessor.keyStartIndex[key];
    int count = audioProcessor.harmonicCounts[key];

    for (int h = 0; h < count; ++h)
    {
        audioProcessor.oscillators[startIndex + h].stretchCents = value;
        audioProcessor.oscillators[startIndex + h].recombineTuningCents();
        audioProcessor.oscillators[startIndex + h].needsFrequencyUpdate = true;
    }

    repaint();

    if (selectedKey == key)
        rebuildTuningSliders();

    audioProcessor.lastAppliedStretchC8 = value;
    stretchC8TextBox.giveAwayKeyboardFocus();
}


void SineLabAudioProcessorEditor::applyShapeToStretch (std::function<double(double)> shapeFunction)
{
    int a0Value = stretchA0TextBox.getText().getIntValue();
    int c8Value = stretchC8TextBox.getText().getIntValue();

    for (int key = 0; key < 88; ++key)
    {
        double keyFraction = (double) key / 87.0;
        double t = keyFraction * 2.0 - 1.0;
        int targetValue = (int) (a0Value + (c8Value - a0Value) * shapeFunction (t));

        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = 0; h < count; ++h)
        {
            audioProcessor.oscillators[startIndex + h].stretchCents = targetValue;
            audioProcessor.oscillators[startIndex + h].recombineTuningCents();
            audioProcessor.oscillators[startIndex + h].needsFrequencyUpdate = true;
        }
    }

    repaintGraphArea();

    if (selectedKey != -1)
        rebuildTuningSliders();
}

void SineLabAudioProcessorEditor::applyShapeToInharmonicity (std::function<double(double)> shapeFunction)
{
    double a0B = stretchA0TextBox.getText().getDoubleValue();
    double c8B = stretchC8TextBox.getText().getDoubleValue();

    for (int key = 0; key < 88; ++key)
    {
        double keyFraction = (double) key / 87.0;
        double t = keyFraction * 2.0 - 1.0;
        double B = a0B + (c8B - a0B) * shapeFunction (t);

        audioProcessor.keyInharmonicityB[key] = B;

        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = 0; h < count; ++h)
        {
            int harmonicNumber = h + 1;
            double magnitudeTerm = std::max (0.0001, 1.0 + B * harmonicNumber * harmonicNumber);
            double cents = 1200.0 * std::log2 (std::sqrt (magnitudeTerm));

            audioProcessor.oscillators[startIndex + h].inharmonicityCents = (int) cents;
            audioProcessor.oscillators[startIndex + h].recombineTuningCents();
            audioProcessor.oscillators[startIndex + h].needsFrequencyUpdate = true;
        }

        recalculateKeyVolume (key);
    }

    repaintGraphArea();

    if (selectedKey != -1)
        rebuildTuningSliders();
}

void SineLabAudioProcessorEditor::applyShapeToField (std::function<double(double)> shapeFunction,
                                                      double a0, double c8,
                                                      double minVal, double maxVal,
                                                      std::function<void(OscillatorState&, double)> setter,
                                                      std::function<void()> rebuildFn)
{
    for (int key = 0; key < 88; ++key)
    {
        double t = ((double) key / 87.0) * 2.0 - 1.0;
        double targetValue = juce::jlimit (minVal, maxVal, a0 + (c8 - a0) * shapeFunction (t));

        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = 0; h < count; ++h)
            setter (audioProcessor.oscillators[startIndex + h], targetValue);
    }

    repaintGraphArea();

    if (selectedKey != -1)
        rebuildFn();
}

void SineLabAudioProcessorEditor::applyShapeToPhase (std::function<double(double)> shapeFunction)
{
    int a0 = juce::jlimit (1, 360, phaseA0TextBox.getText().getIntValue());
    int c8 = juce::jlimit (1, 360, phaseC8TextBox.getText().getIntValue());
    for (int key = 0; key < 88; ++key)
    {
        double t = (double) key / 87.0 * 2.0 - 1.0;
        int value = juce::jlimit (1, 360, (int) std::round (a0 + (c8 - a0) * shapeFunction (t)));
        int start = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];
        for (int h = 0; h < count; ++h)
        {
            auto& osc = audioProcessor.oscillators[start + h];
            osc.startPhaseDegrees = value;
            osc.setStartPhaseDegrees = value;
            osc.startPhase = value * (juce::MathConstants<double>::pi / 180.0);
            osc.needsPhaseUpdate = true;
        }
        recalculateKeyVolume (key);
    }
    repaintGraphArea();
}

void SineLabAudioProcessorEditor::applyShapeToAttack (std::function<double(double)> shapeFunction)
{
    applyShapeToField (shapeFunction,
                       attackA0TextBox.getText().getDoubleValue(), attackC8TextBox.getText().getDoubleValue(),
                       0.0, 1.0,
                       [] (OscillatorState& osc, double v) { osc.attackTime = osc.setAttackTime = v; },
                       [this] { rebuildAttackSliders(); });
}

void SineLabAudioProcessorEditor::applyShapeToSustain (std::function<double(double)> shapeFunction)
{
    double a0       = sustainA0TextBox.getText().getDoubleValue();
    double c8       = sustainC8TextBox.getText().getDoubleValue();
    int    startKey = audioProcessor.lastAppliedSustainStartKey;
    int    endKey   = audioProcessor.lastAppliedSustainEndKey;
    int    span     = endKey - startKey;

    for (int key = 0; key < 88; ++key)
    {
        double v;
        if (key <= startKey)
            v = a0;
        else if (key >= endKey)
            v = c8;
        else
        {
            double t = (span > 0) ? (double) (key - startKey) / span * 2.0 - 1.0 : -1.0;
            v = juce::jlimit (0.0, 1.0, a0 + (c8 - a0) * shapeFunction (t));
        }

        int si = audioProcessor.keyStartIndex[key];
        for (int h = 0; h < audioProcessor.harmonicCounts[key]; ++h)
            audioProcessor.oscillators[si + h].sustainLevel = v;
    }

    repaintGraphArea();
    if (selectedKey != -1) rebuildSustainSliders();
}

void SineLabAudioProcessorEditor::applyShapeToDecay (std::function<double(double)> shapeFunction)
{
    double a0 = decayA0TextBox.getText().getDoubleValue();
    double c8 = decayC8TextBox.getText().getDoubleValue();
    for (int key = 0; key < 88; ++key)
    {
        double t = ((double) key / 87.0) * 2.0 - 1.0;
        double fundDecay = juce::jlimit (0.0, 10.0, a0 + (c8 - a0) * shapeFunction (t));
        audioProcessor.oscillators[audioProcessor.keyStartIndex[key]].decayTime = fundDecay;
        reapplyDecayShapeForKey (key);
    }
    repaintGraphArea();
    if (selectedKey != -1) rebuildDecaySliders();
}

void SineLabAudioProcessorEditor::applyShapeToDecayHarmonics (std::function<double(double)> shapeFunction)
{
    double threshold = audioProcessor.lastAppliedDecayIIThreshold;

    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];
        double fundDecay = audioProcessor.oscillators[startIndex].decayTime;
        int cutoff = (count > 1) ? (int) std::round (threshold * (count - 1)) : 0;

        for (int h = 0; h < count; ++h)
        {
            if (h <= cutoff)
            {
                audioProcessor.oscillators[startIndex + h].decayShapeRatio = 1.0;
                audioProcessor.oscillators[startIndex + h].decayTime = fundDecay;
            }
            else
            {
                int span = count - 1 - cutoff;
                double t = (span > 0) ? ((double)(h - cutoff) / span) * 2.0 - 1.0 : 1.0;
                double ratio = juce::jlimit (0.0, 1.0, 1.0 - shapeFunction (t));
                audioProcessor.oscillators[startIndex + h].decayShapeRatio = ratio;
                audioProcessor.oscillators[startIndex + h].decayTime = fundDecay * ratio;
            }
        }
    }

    repaintGraphArea();

    if (selectedKey != -1)
        rebuildDecaySliders();
}

void SineLabAudioProcessorEditor::applyShapeToKeyVolume (std::function<double(double)> shapeFunction)
{
    double a0      = ampA0TextBox.getText().getDoubleValue();
    double c8      = ampC8TextBox.getText().getDoubleValue();
    int startKey   = audioProcessor.lastAppliedKeyVolumeStartKey;
    int endKey     = audioProcessor.lastAppliedKeyVolumeEndKey;
    int span       = endKey - startKey;
    for (int key = startKey; key <= endKey; ++key)
    {
        double t = (span > 0) ? ((double)(key - startKey) / span) * 2.0 - 1.0 : -1.0;
        audioProcessor.keyVolume[key] = juce::jlimit (0.0, 1.0, a0 + (c8 - a0) * shapeFunction (t));
    }
    repaintGraphArea();
    if (selectedKey != -1) rebuildAmplitudeSliders();
}

void SineLabAudioProcessorEditor::applyShapeToEvenMorph (std::function<double(double)> shapeFunction)
{
    int startKey = audioProcessor.lastAppliedEvenMorphStartKey;
    int endKey   = audioProcessor.lastAppliedEvenMorphEndKey;
    int span     = endKey - startKey;

    double a0 = 1.0;
    double c8 = 0.0;

    for (int key = 0; key < 88; ++key)
    {
        double evenStrength;
        if (key <= startKey)
            evenStrength = a0;
        else if (key >= endKey)
            evenStrength = c8;
        else
        {
            double t = (span > 0) ? ((double)(key - startKey) / span) * 2.0 - 1.0 : -1.0;
            evenStrength = a0 + (c8 - a0) * shapeFunction (t);
        }
        audioProcessor.evenMorphStrength[key] = evenStrength;

        int base  = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];
        for (int h = 0; h < count; ++h)
        {
            int n = audioProcessor.oscillators[base + h].harmonicNumber;
            double amp = 1.0 / n;
            if (n % 2 == 0)
                amp *= evenStrength;
            audioProcessor.oscillators[base + h].amplitude = amp;
        }
        recalculateKeyVolume (key);
    }
    repaintGraphArea();
    if (selectedKey != -1) rebuildAmplitudeSliders();
}

void SineLabAudioProcessorEditor::applyShapeToNToNSquared (std::function<double(double)> shapeFunction)
{
    int startKey = audioProcessor.lastAppliedNToNSquaredStartKey;
    int endKey   = audioProcessor.lastAppliedNToNSquaredEndKey;
    int span     = endKey - startKey;

    for (int key = 0; key < 88; ++key)
    {
        double v;
        if (key <= startKey)
            v = 0.0;
        else if (key >= endKey)
            v = 1.0;
        else
        {
            double t = (span > 0) ? ((double)(key - startKey) / span) * 2.0 - 1.0 : -1.0;
            v = shapeFunction (t);
        }

        int base  = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];
        for (int h = 0; h < count; ++h)
        {
            int n = audioProcessor.oscillators[base + h].harmonicNumber;
            double amp1 = 1.0 / n;
            double amp2 = 1.0 / (n * n);
            audioProcessor.oscillators[base + h].amplitude = (1.0 - v) * amp1 + v * amp2;
        }
        audioProcessor.evenMorphStrength[key] = v;
        recalculateKeyVolume (key);
    }
    repaintGraphArea();
    if (selectedKey != -1) rebuildAmplitudeSliders();
}

void SineLabAudioProcessorEditor::applyShapeToToggle (std::function<double(double)> shapeFunction)
{
    int a0i = juce::jlimit (1, 727, toggleA0TextBox.getText().getIntValue());
    int c8i = juce::jlimit (1,   4, toggleC8TextBox.getText().getIntValue());
    audioProcessor.lastAppliedToggleA0 = a0i;
    audioProcessor.lastAppliedToggleC8 = c8i;
    double a0 = (double) a0i;
    double c8 = (double) c8i;

    for (int key = 0; key < 88; ++key)
    {
        double t       = ((double) key / 87.0) * 2.0 - 1.0;
        double interp  = a0 + (c8 - a0) * shapeFunction (t);
        int    ceiling = juce::jlimit (1, audioProcessor.harmonicCounts[key], (int) std::round (interp));

        int base  = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];
        for (int i = 0; i < count; ++i)
            audioProcessor.oscillators[base + i].aboveCeiling = (i >= ceiling);
        recalculateKeyVolume (key);
    }
    audioProcessor.updateActiveRanks();
    toggleGraphComponent.repaint();
    repaintGraphArea();
}

void SineLabAudioProcessorEditor::applyShapeToAmp (std::function<double(double)> shapeFunction)
{
    double a0 = ampA0TextBox.getText().getDoubleValue();
    double c8 = ampC8TextBox.getText().getDoubleValue();
    int startKey = audioProcessor.lastAppliedAmpStartKey;
    int endKey   = audioProcessor.lastAppliedAmpEndKey;
    int span     = endKey - startKey;
    double alpha = juce::jlimit (0.0, 1.0, integralTextBox.getText().getDoubleValue());
    for (int key = 0; key < 88; ++key)
    {
        double D;
        if (key < startKey)
            D = a0;
        else if (key > endKey)
            D = c8;
        else
        {
            double t = (span > 0) ? ((double)(key - startKey) / span) * 2.0 - 1.0 : -1.0;
            D = a0 + (c8 - a0) * shapeFunction (t);
        }
        applyPulseWaveToKey (audioProcessor, key, juce::jlimit (0.0, 0.5, D), alpha);
    }
    for (int key = 0; key < 88; ++key)
        recalculateKeyVolume (key);
    repaintGraphArea();
    if (selectedKey != -1) rebuildAmplitudeSliders();
}

void SineLabAudioProcessorEditor::wireShapeButtons()
{
    auto makeExpShape = [] (int k) -> std::function<double(double)>
    {
        return [k] (double t)
        {
            double u = (t + 1.0) / 2.0;
            if (k == 0) return u;
            return (std::exp (k * u) - 1.0) / (std::exp ((double) k) - 1.0);
        };
    };

    if (activeTab == 0)
    {
        linearShapeButton.onClick  = [this] { applyShapeToToggle ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick   = [this] { applyShapeToToggle ([] (double t) { double u = (t + 1.0) / 2.0; return 3.0*u*u - 2.0*u*u*u; }); };
        squaredShapeButton.onClick  = nullptr;
        absValueShapeButton.onClick = nullptr;
        expShapeButton.onClick = [this, makeExpShape] { applyShapeToToggle (makeExpShape (audioProcessor.lastAppliedExpKToggle)); };
    }
    else if (activeTab == 1 && activeAmpSubTab == 1)
    {
        linearShapeButton.onClick   = [this] { applyShapeToAmp ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] { applyShapeToAmp ([] (double t) { double u = (t + 1.0) / 2.0; return 3.0*u*u - 2.0*u*u*u; }); };
        squaredShapeButton.onClick  = [this] {
            double a0 = ampA0TextBox.getText().getDoubleValue();
            double c8 = ampC8TextBox.getText().getDoubleValue();
            int startKey = audioProcessor.lastAppliedAmpStartKey;
            int endKey   = audioProcessor.lastAppliedAmpEndKey;
            int span     = endKey - startKey;
            double alpha = juce::jlimit (0.0, 1.0, integralTextBox.getText().getDoubleValue());
            for (int key = 0; key < 88; ++key)
            {
                double D;
                if (key < startKey)
                    D = a0;
                else if (key > endKey)
                    D = c8;
                else
                {
                    double t = (span > 0) ? (double)(key - startKey)/span*2.0-1.0 : -1.0;
                    D = a0*t*t;
                }
                applyPulseWaveToKey (audioProcessor, key, juce::jlimit (0.0, 0.5, D), alpha);
            }
            for (int key = 0; key < 88; ++key) recalculateKeyVolume (key);
            repaintGraphArea(); if (selectedKey != -1) rebuildAmplitudeSliders();
        };
        absValueShapeButton.onClick = [this] {
            double a0 = ampA0TextBox.getText().getDoubleValue();
            double c8 = ampC8TextBox.getText().getDoubleValue();
            int startKey = audioProcessor.lastAppliedAmpStartKey;
            int endKey   = audioProcessor.lastAppliedAmpEndKey;
            int span     = endKey - startKey;
            double alpha = juce::jlimit (0.0, 1.0, integralTextBox.getText().getDoubleValue());
            for (int key = 0; key < 88; ++key)
            {
                double D;
                if (key < startKey)
                    D = a0;
                else if (key > endKey)
                    D = c8;
                else
                {
                    double t = (span > 0) ? (double)(key - startKey)/span*2.0-1.0 : -1.0;
                    D = a0*std::abs(t);
                }
                applyPulseWaveToKey (audioProcessor, key, juce::jlimit (0.0, 0.5, D), alpha);
            }
            for (int key = 0; key < 88; ++key) recalculateKeyVolume (key);
            repaintGraphArea(); if (selectedKey != -1) rebuildAmplitudeSliders();
        };


        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToAmp (makeExpShape (audioProcessor.lastAppliedExpKAmp)); };
    }
    else if (activeTab == 1 && activeAmpSubTab == 2)
    {
        linearShapeButton.onClick   = [this] { applyShapeToKeyVolume ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] { applyShapeToKeyVolume ([] (double t) { double u = (t + 1.0) / 2.0; return 3.0*u*u - 2.0*u*u*u; }); };
        squaredShapeButton.onClick  = [this] {
            double a0 = ampA0TextBox.getText().getDoubleValue();
            int startKey = audioProcessor.lastAppliedKeyVolumeStartKey;
            int endKey   = audioProcessor.lastAppliedKeyVolumeEndKey;
            int span     = endKey - startKey;
            for (int key = startKey; key <= endKey; ++key) { double t = (span > 0) ? (double)(key - startKey)/span*2.0-1.0 : -1.0; audioProcessor.keyVolume[key] = juce::jlimit (0.0, 1.0, a0*t*t); }
            repaintGraphArea(); if (selectedKey != -1) rebuildAmplitudeSliders();
        };
        absValueShapeButton.onClick = [this] {
            double a0 = ampA0TextBox.getText().getDoubleValue();
            int startKey = audioProcessor.lastAppliedKeyVolumeStartKey;
            int endKey   = audioProcessor.lastAppliedKeyVolumeEndKey;
            int span     = endKey - startKey;
            for (int key = startKey; key <= endKey; ++key) { double t = (span > 0) ? (double)(key - startKey)/span*2.0-1.0 : -1.0; audioProcessor.keyVolume[key] = juce::jlimit (0.0, 1.0, a0*std::abs(t)); }
            repaintGraphArea(); if (selectedKey != -1) rebuildAmplitudeSliders();
        };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToKeyVolume (makeExpShape (audioProcessor.lastAppliedExpKKeyVolume)); };
    }
    else if (activeTab == 1 && activeAmpSubTab == 3)
    {
        linearShapeButton.onClick   = [this] { applyShapeToEvenMorph ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] { applyShapeToEvenMorph ([] (double t) { double u = (t + 1.0) / 2.0; return 3.0*u*u - 2.0*u*u*u; }); };
        squaredShapeButton.onClick  = [this] { applyShapeToEvenMorph ([] (double t) { return t * t; }); };
        absValueShapeButton.onClick = [this] { applyShapeToEvenMorph ([] (double t) { return std::abs (t); }); };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToEvenMorph (makeExpShape (audioProcessor.lastAppliedExpKEvenMorph)); };

        evenMorphLinearCenterButton.onClick   = [this] { applyShapeToNToNSquared ([] (double t) { return (t + 1.0) / 2.0; }); };
        evenMorphCubicCenterButton.onClick    = [this] { applyShapeToNToNSquared ([] (double t) { double u = (t + 1.0) / 2.0; return 3.0*u*u - 2.0*u*u*u; }); };
        evenMorphSquaredCenterButton.onClick  = [this] { applyShapeToNToNSquared ([] (double t) { return t * t; }); };
        evenMorphAbsValueCenterButton.onClick = [this] { applyShapeToNToNSquared ([] (double t) { return std::abs (t); }); };
        evenMorphExpCenterButton.onClick      = [this, makeExpShape] {
            int k = evenMorphExpSteepnessCenterTextBox.getText().getIntValue();
            applyShapeToNToNSquared (makeExpShape (k));
        };
    }
    else if (activeTab == 2 && activeTuningSubTab == 1)
    {
        linearShapeButton.onClick   = [this] { applyShapeToStretch ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] {
            int a0 = stretchA0TextBox.getText().getIntValue(), c8 = stretchC8TextBox.getText().getIntValue();
            int mid = (a0+c8)/2, half = (c8-a0)/2;
            for (int key = 0; key < 88; ++key) {
                double t = (double)key/87.0*2.0-1.0;
                int target = mid + (int)(half*t*t*t);
                int si = audioProcessor.keyStartIndex[key], count = audioProcessor.harmonicCounts[key];
                for (int h = 0; h < count; ++h) { audioProcessor.oscillators[si+h].stretchCents = target; audioProcessor.oscillators[si+h].recombineTuningCents(); audioProcessor.oscillators[si+h].needsFrequencyUpdate = true; }
            }
            repaintGraphArea(); if (selectedKey != -1) rebuildTuningSliders();
        };
        squaredShapeButton.onClick  = [this] { applyShapeToStretch ([] (double t) { return t * t; }); };
        absValueShapeButton.onClick = [this] { applyShapeToStretch ([] (double t) { return std::abs (t); }); };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToStretch (makeExpShape (audioProcessor.lastAppliedExpKTuning1)); };
    }
    else if (activeTab == 2 && activeTuningSubTab == 2)
    {
        linearShapeButton.onClick   = [this] { applyShapeToInharmonicity ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] {
            double a0B = stretchA0TextBox.getText().getDoubleValue(), c8B = stretchC8TextBox.getText().getDoubleValue();
            double mid = (a0B+c8B)/2.0, half = (c8B-a0B)/2.0;
            for (int key = 0; key < 88; ++key) {
                double t = (double)key/87.0*2.0-1.0;
                double B = mid + half*t*t*t;
                audioProcessor.keyInharmonicityB[key] = B;
                int si = audioProcessor.keyStartIndex[key], count = audioProcessor.harmonicCounts[key];
                for (int h = 0; h < count; ++h) { int hn=h+1; double mag=std::max(0.0001,1.0+B*hn*hn); double cents=1200.0*std::log2(std::sqrt(mag)); audioProcessor.oscillators[si+h].inharmonicityCents=(int)cents; audioProcessor.oscillators[si+h].recombineTuningCents(); audioProcessor.oscillators[si+h].needsFrequencyUpdate=true; }
                recalculateKeyVolume (key);
            }
            repaintGraphArea(); if (selectedKey != -1) rebuildTuningSliders();
        };
        squaredShapeButton.onClick  = [this] { applyShapeToInharmonicity ([] (double t) { return t * t; }); };
        absValueShapeButton.onClick = [this] { applyShapeToInharmonicity ([] (double t) { return std::abs (t); }); };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToInharmonicity (makeExpShape (audioProcessor.lastAppliedExpKTuning2)); };
    }
    else if (activeTab == 3)
    {
        linearShapeButton.onClick   = [this] { applyShapeToPhase ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] { applyShapeToPhase ([] (double t) { double u = (t + 1.0) / 2.0; return 3.0*u*u - 2.0*u*u*u; }); };
        squaredShapeButton.onClick  = [this] { applyShapeToPhase ([] (double t) { return t * t; }); };
        absValueShapeButton.onClick = [this] { applyShapeToPhase ([] (double t) { return std::abs (t); }); };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToPhase (makeExpShape (audioProcessor.lastAppliedExpKPhase)); };
    }
    else if (activeTab == 4)
    {
        linearShapeButton.onClick   = nullptr;
        cubicShapeButton.onClick    = nullptr;
        squaredShapeButton.onClick  = nullptr;
        absValueShapeButton.onClick = nullptr;
        expShapeButton.onClick      = nullptr;
    }
    else if (activeTab == 5)
    {
        linearShapeButton.onClick   = [this] { applyShapeToAttack ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] { applyShapeToAttack ([] (double t) { double u=(t+1.0)/2.0; return 3.0*u*u-2.0*u*u*u; }); };
        squaredShapeButton.onClick  = [this] {
            double a0 = attackA0TextBox.getText().getDoubleValue();
            for (int key = 0; key < 88; ++key) { double t=(double)key/87.0*2.0-1.0; double v=juce::jlimit(0.0,1.0,a0*t*t); int si=audioProcessor.keyStartIndex[key]; for (int h=0;h<audioProcessor.harmonicCounts[key];++h) audioProcessor.oscillators[si+h].attackTime=audioProcessor.oscillators[si+h].setAttackTime=v; }
            repaintGraphArea(); if (selectedKey != -1) rebuildAttackSliders();
        };
        absValueShapeButton.onClick = [this] {
            double a0 = attackA0TextBox.getText().getDoubleValue();
            for (int key = 0; key < 88; ++key) { double t=(double)key/87.0*2.0-1.0; double v=juce::jlimit(0.0,1.0,a0*std::abs(t)); int si=audioProcessor.keyStartIndex[key]; for (int h=0;h<audioProcessor.harmonicCounts[key];++h) audioProcessor.oscillators[si+h].attackTime=audioProcessor.oscillators[si+h].setAttackTime=v; }
            repaintGraphArea(); if (selectedKey != -1) rebuildAttackSliders();
        };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToAttack (makeExpShape (audioProcessor.lastAppliedExpKAttack)); };
    }
    else if (activeTab == 6 && activeDecaySubTab == 1)
    {
        linearShapeButton.onClick   = [this] { applyShapeToDecay ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] { applyShapeToDecay ([] (double t) { double u=(t+1.0)/2.0; return 3.0*u*u-2.0*u*u*u; }); };
        squaredShapeButton.onClick  = [this] {
            double a0 = decayA0TextBox.getText().getDoubleValue();
            for (int key = 0; key < 88; ++key) { double t=(double)key/87.0*2.0-1.0; audioProcessor.oscillators[audioProcessor.keyStartIndex[key]].decayTime=juce::jlimit(0.0,10.0,a0*t*t); reapplyDecayShapeForKey(key); }
            repaintGraphArea(); if (selectedKey != -1) rebuildDecaySliders();
        };
        absValueShapeButton.onClick = [this] {
            double a0 = decayA0TextBox.getText().getDoubleValue();
            for (int key = 0; key < 88; ++key) { double t=(double)key/87.0*2.0-1.0; audioProcessor.oscillators[audioProcessor.keyStartIndex[key]].decayTime=juce::jlimit(0.0,10.0,a0*std::abs(t)); reapplyDecayShapeForKey(key); }
            repaintGraphArea(); if (selectedKey != -1) rebuildDecaySliders();
        };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToDecay (makeExpShape (audioProcessor.lastAppliedExpKDecay1)); };
    }
    else if (activeTab == 6 && activeDecaySubTab == 2)
    {
        linearShapeButton.onClick   = [this] { lastDecayIIShape = [] (double t) { return (t + 1.0) / 2.0; };                                          applyShapeToDecayHarmonics (lastDecayIIShape); };
        cubicShapeButton.onClick    = [this] { lastDecayIIShape = [] (double t) { double u=(t+1.0)/2.0; return 3.0*u*u-2.0*u*u*u; };                  applyShapeToDecayHarmonics (lastDecayIIShape); };
        squaredShapeButton.onClick  = nullptr;
        absValueShapeButton.onClick = nullptr;
        expShapeButton.onClick      = [this, makeExpShape] { lastDecayIIShape = makeExpShape (audioProcessor.lastAppliedExpKDecay2); applyShapeToDecayHarmonics (lastDecayIIShape); };
    }
    else if (activeTab == 7)
    {
        sustainLinearButton.onClick   = [this] { applyShapeToSustain ([] (double t) { return (t + 1.0) / 2.0; }); };
        sustainCubicButton.onClick    = [this] { applyShapeToSustain ([] (double t) { double u=(t+1.0)/2.0; return 3.0*u*u-2.0*u*u*u; }); };
        sustainSquaredButton.onClick  = [this] {
            double a0 = sustainA0TextBox.getText().getDoubleValue();
            int startKey = audioProcessor.lastAppliedSustainStartKey;
            int endKey   = audioProcessor.lastAppliedSustainEndKey;
            int span     = endKey - startKey;
            for (int key = 0; key < 88; ++key)
            {
                double v;
                if (key <= startKey) v = a0;
                else if (key >= endKey) v = sustainC8TextBox.getText().getDoubleValue();
                else { double t = (span > 0) ? (double)(key-startKey)/span*2.0-1.0 : -1.0; v = juce::jlimit(0.0,1.0,a0*t*t); }
                int si = audioProcessor.keyStartIndex[key];
                for (int h = 0; h < audioProcessor.harmonicCounts[key]; ++h)
                    audioProcessor.oscillators[si+h].sustainLevel = v;
            }
            repaintGraphArea(); if (selectedKey != -1) rebuildSustainSliders();
        };
        sustainAbsValueButton.onClick = [this] {
            double a0 = sustainA0TextBox.getText().getDoubleValue();
            int startKey = audioProcessor.lastAppliedSustainStartKey;
            int endKey   = audioProcessor.lastAppliedSustainEndKey;
            int span     = endKey - startKey;
            for (int key = 0; key < 88; ++key)
            {
                double v;
                if (key <= startKey) v = a0;
                else if (key >= endKey) v = sustainC8TextBox.getText().getDoubleValue();
                else { double t = (span > 0) ? (double)(key-startKey)/span*2.0-1.0 : -1.0; v = juce::jlimit(0.0,1.0,a0*std::abs(t)); }
                int si = audioProcessor.keyStartIndex[key];
                for (int h = 0; h < audioProcessor.harmonicCounts[key]; ++h)
                    audioProcessor.oscillators[si+h].sustainLevel = v;
            }
            repaintGraphArea(); if (selectedKey != -1) rebuildSustainSliders();
        };
        sustainExpButton.onClick      = [this, makeExpShape] { applyShapeToSustain (makeExpShape (audioProcessor.lastAppliedExpKSustain)); };
    }
    else if (activeTab == 8)
    {
        linearShapeButton.onClick   = [this] { applyShapeToRelease ([] (double t) { return (t + 1.0) / 2.0; }); };
        cubicShapeButton.onClick    = [this] { applyShapeToRelease ([] (double t) { double u=(t+1.0)/2.0; return 3.0*u*u-2.0*u*u*u; }); };
        squaredShapeButton.onClick  = [this] {
            double a0 = releaseA0TextBox.getText().getDoubleValue();
            for (int key = 0; key < 88; ++key) { double t=(double)key/87.0*2.0-1.0; double v=juce::jlimit(0.0,1.0,a0*t*t); int si=audioProcessor.keyStartIndex[key]; for (int h=0;h<audioProcessor.harmonicCounts[key];++h) audioProcessor.oscillators[si+h].releaseTime=audioProcessor.oscillators[si+h].setReleaseTime=v; }
            repaintGraphArea(); if (selectedKey != -1) rebuildReleaseSliders();
        };
        absValueShapeButton.onClick = [this] {
            double a0 = releaseA0TextBox.getText().getDoubleValue();
            for (int key = 0; key < 88; ++key) { double t=(double)key/87.0*2.0-1.0; double v=juce::jlimit(0.0,1.0,a0*std::abs(t)); int si=audioProcessor.keyStartIndex[key]; for (int h=0;h<audioProcessor.harmonicCounts[key];++h) audioProcessor.oscillators[si+h].releaseTime=audioProcessor.oscillators[si+h].setReleaseTime=v; }
            repaintGraphArea(); if (selectedKey != -1) rebuildReleaseSliders();
        };
        expShapeButton.onClick      = [this, makeExpShape] { applyShapeToRelease (makeExpShape (audioProcessor.lastAppliedExpKRelease)); };
    }
    else
    {
        linearShapeButton.onClick   = nullptr;
        cubicShapeButton.onClick    = nullptr;
        squaredShapeButton.onClick  = nullptr;
        absValueShapeButton.onClick = nullptr;
        expShapeButton.onClick      = nullptr;
    }
}

void SineLabAudioProcessorEditor::expSteepnessApplied()
{
    int value = juce::jlimit (-50, 50, expSteepnessTextBox.getText().getIntValue());
    expSteepnessTextBox.setText (juce::String (value), juce::dontSendNotification);
    if (activeTab == 0)
        audioProcessor.lastAppliedExpKToggle = value;
    else if (activeTab == 1 && activeAmpSubTab == 1)
        audioProcessor.lastAppliedExpKAmp = value;
    else if (activeTab == 1 && activeAmpSubTab == 2)
        audioProcessor.lastAppliedExpKKeyVolume = value;
    else if (activeTab == 1 && activeAmpSubTab == 3)
        audioProcessor.lastAppliedExpKEvenMorph = value;
    else if (activeTab == 2 && activeTuningSubTab == 1)
        audioProcessor.lastAppliedExpKTuning1 = value;
    else if (activeTab == 2 && activeTuningSubTab == 2)
        audioProcessor.lastAppliedExpKTuning2 = value;
    else if (activeTab == 5)
        audioProcessor.lastAppliedExpKAttack = value;
    else if (activeTab == 6 && activeDecaySubTab == 1)
        audioProcessor.lastAppliedExpKDecay1 = value;
    else if (activeTab == 6 && activeDecaySubTab == 2)
        audioProcessor.lastAppliedExpKDecay2 = value;
    else if (activeTab == 3)
        audioProcessor.lastAppliedExpKPhase = value;
    else if (activeTab == 8)
        audioProcessor.lastAppliedExpKRelease = value;
    expSteepnessTextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::globalPhaseApplied()
{
    int value = juce::jlimit (1, 360, globalPhaseTextBox.getText().getIntValue());
    globalPhaseTextBox.setText (juce::String (value), juce::dontSendNotification);
    audioProcessor.globalPhaseValue = value;

    for (auto& osc : audioProcessor.oscillators)
    {
        osc.startPhaseDegrees = value;
        osc.setStartPhaseDegrees = value;
        osc.startPhase = value * (juce::MathConstants<double>::pi / 180.0);
        osc.needsPhaseUpdate = true;
    }

    recalculateAllKeyVolumes();
    repaint();
    globalPhaseTextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::globalPanApplied()
{
    double value = juce::jlimit (-1.0, 1.0, globalPanTextBox.getText().getDoubleValue());
    globalPanTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.globalPanValue = value;

    for (auto& osc : audioProcessor.oscillators)
    {
        osc.pan = value;
        osc.needsPanUpdate = true;
    }

    globalPanTextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::evenLeftOddRightClicked()
{
    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        double w = audioProcessor.lastAppliedPanWidth;
        for (int h = 1; h < count; ++h)
        {
            audioProcessor.oscillators[startIndex + h].pan = (h % 2 == 1) ? -w : w;
            audioProcessor.oscillators[startIndex + h].needsPanUpdate = true;
        }
    }

    if (selectedKey != -1)
        rebuildPanSliders();
}

void SineLabAudioProcessorEditor::oddLeftEvenRightClicked()
{
    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        double w = audioProcessor.lastAppliedPanWidth;
        for (int h = 1; h < count; ++h)
        {
            int harmonicNumber = h + 1;
            audioProcessor.oscillators[startIndex + h].pan = (harmonicNumber % 2 == 1) ? -w : w;
            audioProcessor.oscillators[startIndex + h].needsPanUpdate = true;
        }
    }

    if (selectedKey != -1)
        rebuildPanSliders();
}

void SineLabAudioProcessorEditor::twoLeftClicked()
{
    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = 1; h < count; ++h)
        {
            int harmonicNumber = h + 1;
            int group = (harmonicNumber - 2) / 2;
            double w = audioProcessor.lastAppliedPanWidth;
            audioProcessor.oscillators[startIndex + h].pan = (group % 2 == 0) ? -w : w;
            audioProcessor.oscillators[startIndex + h].needsPanUpdate = true;
        }
    }

    if (selectedKey != -1)
        rebuildPanSliders();
}

void SineLabAudioProcessorEditor::alternateActiveClicked()
{
    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];
        bool panLeft = true;

        for (int h = 1; h < count; ++h)
        {
            auto& osc = audioProcessor.oscillators[startIndex + h];

            if (! osc.isAudible())
                continue;

            osc.pan = panLeft ? -audioProcessor.lastAppliedPanWidth : audioProcessor.lastAppliedPanWidth;
            osc.needsPanUpdate = true;
            panLeft = ! panLeft;
        }
    }

    if (selectedKey != -1)
        rebuildPanSliders();
}

void SineLabAudioProcessorEditor::smartPanClicked()
{
    double w = audioProcessor.lastAppliedPanWidth;
    for (int key = 0; key < 88; ++key)
    {
        int startIndex = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];
        if (count <= 0)
            continue;

        audioProcessor.oscillators[startIndex].pan = 0.0;
        audioProcessor.oscillators[startIndex].needsPanUpdate = true;

        if (count < 2)
            continue;

        struct Overtone {
            int oscIndex;
            double amplitude;
        };
        std::vector<Overtone> activeOvertones;
        for (int h = 1; h < count; ++h)
        {
            auto& osc = audioProcessor.oscillators[startIndex + h];
            if (osc.amplitude > 1e-5)
            {
                activeOvertones.push_back ({ startIndex + h, osc.amplitude });
            }
        }

        if (activeOvertones.empty())
            continue;

        std::sort (activeOvertones.begin(), activeOvertones.end(), [] (const Overtone& a, const Overtone& b) {
            return a.amplitude > b.amplitude;
        });

        double leftSum = 0.0;
        double rightSum = 0.0;

        for (const auto& ov : activeOvertones)
        {
            if (leftSum <= rightSum)
            {
                audioProcessor.oscillators[ov.oscIndex].pan = -w;
                leftSum += ov.amplitude;
            }
            else
            {
                audioProcessor.oscillators[ov.oscIndex].pan = w;
                rightSum += ov.amplitude;
            }
            audioProcessor.oscillators[ov.oscIndex].needsPanUpdate = true;
        }
    }

    if (selectedKey != -1)
        rebuildPanSliders();
}

void SineLabAudioProcessorEditor::globalAttackApplied()
{
    double value = juce::jlimit (0.0, 1.0, globalAttackTextBox.getText().getDoubleValue());
    
    globalAttackTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.globalAttackValue = value;

    for (auto& osc : audioProcessor.oscillators)
    {
        osc.attackTime = value;
        osc.setAttackTime = value;
    }

    repaint();
    globalAttackTextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::attackA0Applied()
{
    double value = juce::jlimit (0.0, 1.0, attackA0TextBox.getText().getDoubleValue());
    attackA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    int startIndex = audioProcessor.keyStartIndex[0];
    int count = audioProcessor.harmonicCounts[0];
    for (int h = 0; h < count; ++h)
    {
        audioProcessor.oscillators[startIndex + h].attackTime = value;
        audioProcessor.oscillators[startIndex + h].setAttackTime = value;
    }
    repaint();
    audioProcessor.lastAppliedAttackA0 = value;
    attackA0TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::attackC8Applied()
{
    double value = juce::jlimit (0.0, 1.0, attackC8TextBox.getText().getDoubleValue());
    attackC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    int startIndex = audioProcessor.keyStartIndex[87];
    int count = audioProcessor.harmonicCounts[87];
    for (int h = 0; h < count; ++h)
    {
        audioProcessor.oscillators[startIndex + h].attackTime = value;
        audioProcessor.oscillators[startIndex + h].setAttackTime = value;
    }
    repaint();
    audioProcessor.lastAppliedAttackC8 = value;
    attackC8TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::applyShapeToRelease (std::function<double(double)> shapeFunction)
{
    applyShapeToField (shapeFunction,
                       releaseA0TextBox.getText().getDoubleValue(), releaseC8TextBox.getText().getDoubleValue(),
                       0.0, 1.0,
                       [] (OscillatorState& osc, double v) { osc.releaseTime = osc.setReleaseTime = v; },
                       [this] { rebuildReleaseSliders(); });
}

void SineLabAudioProcessorEditor::releaseA0Applied()
{
    double value = juce::jlimit (0.0, 1.0, releaseA0TextBox.getText().getDoubleValue());
    releaseA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    int startIndex = audioProcessor.keyStartIndex[0];
    int count = audioProcessor.harmonicCounts[0];
    for (int h = 0; h < count; ++h)
        audioProcessor.oscillators[startIndex + h].releaseTime = audioProcessor.oscillators[startIndex + h].setReleaseTime = value;
    repaint();
    audioProcessor.lastAppliedReleaseA0 = value;
    releaseA0TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::releaseC8Applied()
{
    double value = juce::jlimit (0.0, 1.0, releaseC8TextBox.getText().getDoubleValue());
    releaseC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    int startIndex = audioProcessor.keyStartIndex[87];
    int count = audioProcessor.harmonicCounts[87];
    for (int h = 0; h < count; ++h)
        audioProcessor.oscillators[startIndex + h].releaseTime = audioProcessor.oscillators[startIndex + h].setReleaseTime = value;
    repaint();
    audioProcessor.lastAppliedReleaseC8 = value;
    releaseC8TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::decayA0Applied()
{
    double value = juce::jlimit (0.0, 10.0, decayA0TextBox.getText().getDoubleValue());
    decayA0TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.oscillators[audioProcessor.keyStartIndex[0]].decayTime = value;
    reapplyDecayShapeForKey (0);
    audioProcessor.lastAppliedDecayA0 = value;
    repaintGraphArea();
    decayA0TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::decayC8Applied()
{
    double value = juce::jlimit (0.0, 10.0, decayC8TextBox.getText().getDoubleValue());
    decayC8TextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.oscillators[audioProcessor.keyStartIndex[87]].decayTime = value;
    reapplyDecayShapeForKey (87);
    audioProcessor.lastAppliedDecayC8 = value;
    repaintGraphArea();
    decayC8TextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::globalDecayApplied()
{
    double value = juce::jlimit (0.0, 20.0, globalDecayTextBox.getText().getDoubleValue());
    globalDecayTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.globalDecayValue = value;

    for (int key = 0; key < 88; ++key)
    {
        int start = audioProcessor.keyStartIndex[key];
        int count = audioProcessor.harmonicCounts[key];

        for (int h = 0; h < count; ++h)
        {
            audioProcessor.oscillators[start + h].decayTime       = value;
            audioProcessor.oscillators[start + h].decayShapeRatio = 1.0;
        }
    }

    repaintGraphArea();
    globalDecayTextBox.giveAwayKeyboardFocus();
}


void SineLabAudioProcessorEditor::globalSustainApplied()
{
    double value = juce::jlimit (0.0, 1.0, globalSustainTextBox.getText().getDoubleValue());
    globalSustainTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.globalSustainValue = value;

    for (auto& osc : audioProcessor.oscillators)
        osc.sustainLevel = value;

    repaint();
    globalSustainTextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::globalReleaseApplied()
{
    double value = juce::jlimit (0.0, 1.0, globalReleaseTextBox.getText().getDoubleValue());
    globalReleaseTextBox.setText (juce::String (value, 4), juce::dontSendNotification);
    audioProcessor.globalReleaseValue = value;

    for (auto& osc : audioProcessor.oscillators)
        osc.releaseTime = osc.setReleaseTime = value;

    repaintGraphArea();
    globalReleaseTextBox.giveAwayKeyboardFocus();
}

void SineLabAudioProcessorEditor::tuningSliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        audioProcessor.oscillators[index].tuningCents = tuningSliders[harmonicIndex]->getValue();
        audioProcessor.oscillators[index].needsFrequencyUpdate = true;
        recalculateKeyVolume (selectedKey);
        repaintGraphArea();
    }
}

void SineLabAudioProcessorEditor::rebuildTuningSliders()
{
    if (selectedKey != -1 && selectedKey == tuningBuiltKey
        && tuningSliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < tuningSliders.size(); ++h)
            tuningSliders[h]->setValue (audioProcessor.oscillators[startIndex + h].tuningCents, juce::dontSendNotification);
        return;
    }

    tuningBuiltKey = -1;
    tuningSliders.clear();
    tuningLabels.clear();

    if (selectedKey == -1)
        return;

    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];

    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f) + 30;
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int sliderHeight = contentAreaBottom - contentAreaTop;

    for (int h = 0; h < count; ++h)
    {
        
        double harmonicFrequency = (h + 1) * (440.0 * std::pow (2.0, (audioProcessor.oscillators[startIndex].midiNoteNumber - 69) / 12.0));

                if (audioProcessor.oscillators[startIndex].midiNoteNumber == 0)
                    harmonicFrequency = (h + 1) * (440.0 * std::pow (2.0, (selectedKey + 21 - 69) / 12.0));

        int audibleMaxCents = juce::jmax (1, (int) (1200.0 * std::log2 (20000.0 / harmonicFrequency)));
                                int maxUpwardCents = audibleMaxCents + 1;
                        
                                int audibleMinCents = juce::jmax (1, (int) (1200.0 * std::log2 (harmonicFrequency / 20.0)));
                                int maxDownwardCents = -(audibleMinCents + 1);

                auto* slider = setupHarmonicSlider (tuningContainer, tuningLabels, maxDownwardCents, maxUpwardCents, 1, h, audioProcessor.oscillators[startIndex + h].tuningCents, [this] (int harmonicIndex) { tuningSliderChanged (harmonicIndex); });
        
        tuningSliders.add (slider);
    }

    tuningContainer.setSize (count * 40, sliderHeight + 12);
    tuningBuiltKey = selectedKey;
}

void SineLabAudioProcessorEditor::phaseSliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        int degrees = (int) phaseSliders[harmonicIndex]->getValue();
        audioProcessor.oscillators[index].startPhaseDegrees = degrees;
        audioProcessor.oscillators[index].setStartPhaseDegrees = degrees;
        audioProcessor.oscillators[index].startPhase = degrees * (juce::MathConstants<double>::pi / 180.0);

        for (int i = 0; i < (int) audioProcessor.oscillators.size(); ++i)
            audioProcessor.oscillators[i].needsPhaseUpdate = true;

        recalculateKeyVolume (selectedKey);
        repaintGraphArea();
    }
}

void SineLabAudioProcessorEditor::rebuildPhaseSliders()
{
    if (selectedKey != -1 && selectedKey == phaseBuiltKey
        && phaseSliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < phaseSliders.size(); ++h)
            phaseSliders[h]->setValue (audioProcessor.oscillators[startIndex + h].startPhaseDegrees, juce::dontSendNotification);
        return;
    }

    phaseBuiltKey = -1;
    phaseSliders.clear();
    phaseLabels.clear();

    if (selectedKey == -1)
        return;

    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];

    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f) + 30;
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int sliderHeight = contentAreaBottom - contentAreaTop;

    for (int h = 0; h < count; ++h)
    {
        auto* slider = setupHarmonicSlider (phaseContainer, phaseLabels, 1.0, 360.0, 1.0, h, audioProcessor.oscillators[startIndex + h].startPhaseDegrees, [this] (int harmonicIndex) { phaseSliderChanged (harmonicIndex); });
        
        
        phaseSliders.add (slider);
    }

    phaseContainer.setSize (count * 40, sliderHeight + 12);
    phaseBuiltKey = selectedKey;
}



void SineLabAudioProcessorEditor::panSliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        audioProcessor.oscillators[index].pan = panSliders[harmonicIndex]->getValue();
        audioProcessor.oscillators[index].needsPanUpdate = true;
    }
}

void SineLabAudioProcessorEditor::rebuildPanSliders()
{
    if (selectedKey != -1 && selectedKey == panBuiltKey
        && panSliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < panSliders.size(); ++h)
            panSliders[h]->setValue (audioProcessor.oscillators[startIndex + h].pan, juce::dontSendNotification);
        return;
    }

    panBuiltKey = -1;
    panSliders.clear();
    panLabels.clear();
    if (selectedKey == -1)
        return;
    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];
    auto bounds = getLocalBounds();
    int fixedSliderWidth = (bounds.getWidth() * 2 / 3) - 20;
    for (int h = 0; h < count; ++h)
    {
        auto* slider = setupHorizontalHarmonicSlider (panContainer, panLabels, -1.0, 1.0, 0.01, h, count, audioProcessor.oscillators[startIndex + h].pan, [this] (int harmonicIndex) { panSliderChanged (harmonicIndex); });
        panSliders.add (slider);
    }
    int viewportHeight = ((bounds.getHeight() - (bounds.getHeight() / 8)) - ((bounds.getHeight() / 16) + 30));
    int neededHeight = juce::jmax (viewportHeight, count * 30);
    panContainer.setSize (fixedSliderWidth + 20, neededHeight);
    panBuiltKey = selectedKey;
}




void SineLabAudioProcessorEditor::attackSliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        audioProcessor.oscillators[index].attackTime = attackSliders[harmonicIndex]->getValue();
        audioProcessor.oscillators[index].setAttackTime = audioProcessor.oscillators[index].attackTime;
        repaintGraphArea();
    }
}

void SineLabAudioProcessorEditor::rebuildAttackSliders()
{
    if (selectedKey != -1 && selectedKey == attackBuiltKey
        && attackSliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < attackSliders.size(); ++h)
            attackSliders[h]->setValue (audioProcessor.oscillators[startIndex + h].attackTime, juce::dontSendNotification);
        return;
    }

    attackBuiltKey = -1;
    attackSliders.clear();
    attackLabels.clear();

    if (selectedKey == -1)
        return;

    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];

    auto bounds = getLocalBounds();
    int fixedSliderWidth = (bounds.getWidth() * 2 / 3) - 20;

    for (int h = 0; h < count; ++h)
    {
        auto* slider = setupHorizontalHarmonicSlider (attackContainer, attackLabels, 0.0, 1.0, 0.0001, h, count, audioProcessor.oscillators[startIndex + h].attackTime, [this] (int harmonicIndex) { attackSliderChanged (harmonicIndex); });


        attackSliders.add (slider);
    }

    int viewportHeight = ((bounds.getHeight() - (bounds.getHeight() / 8)) - ((bounds.getHeight() / 16) + 30));
        int neededHeight = juce::jmax (viewportHeight, count * 30);
        attackContainer.setSize (fixedSliderWidth + 20, neededHeight);
        attackBuiltKey = selectedKey;
    
}


void SineLabAudioProcessorEditor::decaySliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        int index = startIndex + harmonicIndex;
        double newDecay = decaySliders[harmonicIndex]->getValue();
        audioProcessor.oscillators[index].decayTime = newDecay;
        double fundDecay = audioProcessor.oscillators[startIndex].decayTime;
        if (fundDecay > 0.0)
            audioProcessor.oscillators[index].decayShapeRatio = juce::jlimit (0.0, 1.0, newDecay / fundDecay);
        repaintGraphArea();
    }
}

void SineLabAudioProcessorEditor::rebuildDecaySliders()
{
    if (selectedKey != -1 && selectedKey == decayBuiltKey
        && decaySliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < decaySliders.size(); ++h)
            decaySliders[h]->setValue (audioProcessor.oscillators[startIndex + h].decayTime, juce::dontSendNotification);
        return;
    }

    decayBuiltKey = -1;
    decaySliders.clear();
    decayLabels.clear();
    if (selectedKey == -1)
        return;
    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];
    auto bounds = getLocalBounds();
    int fixedSliderWidth = (bounds.getWidth() * 2 / 3) - 20;
    for (int h = 0; h < count; ++h)
    {
        auto* slider = setupHorizontalHarmonicSlider (decayContainer, decayLabels, 0.0, 20.0, 0.0001, h, count, audioProcessor.oscillators[startIndex + h].decayTime, [this] (int harmonicIndex) { decaySliderChanged (harmonicIndex); });
        decaySliders.add (slider);
    }
    int viewportHeight = ((bounds.getHeight() - (bounds.getHeight() / 8)) - ((bounds.getHeight() / 16) + 30));
    int neededHeight = juce::jmax (viewportHeight, count * 30);
    decayContainer.setSize (fixedSliderWidth + 20, neededHeight);
    decayBuiltKey = selectedKey;
}



void SineLabAudioProcessorEditor::sustainSliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        audioProcessor.oscillators[index].sustainLevel = sustainSliders[harmonicIndex]->getValue();
        repaintGraphArea();
    }
}

void SineLabAudioProcessorEditor::rebuildSustainSliders()
{
    if (selectedKey != -1 && selectedKey == sustainBuiltKey
        && sustainSliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < sustainSliders.size(); ++h)
            sustainSliders[h]->setValue (audioProcessor.oscillators[startIndex + h].sustainLevel, juce::dontSendNotification);
        return;
    }

    sustainBuiltKey = -1;
    sustainSliders.clear();
    sustainLabels.clear();
    if (selectedKey == -1)
        return;
    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];
    auto bounds = getLocalBounds();
    int fixedSliderWidth = (bounds.getWidth() * 2 / 3) - 20;
    for (int h = 0; h < count; ++h)
    {
        auto* slider = setupHorizontalHarmonicSlider (sustainContainer, sustainLabels, 0.0, 1.0, 0.0001, h, count, audioProcessor.oscillators[startIndex + h].sustainLevel, [this] (int harmonicIndex) { sustainSliderChanged (harmonicIndex); });
        sustainSliders.add (slider);
    }
    int viewportHeight = ((bounds.getHeight() - (bounds.getHeight() / 8)) - ((bounds.getHeight() / 16) + 30));
    int neededHeight = juce::jmax (viewportHeight, count * 30);
    sustainContainer.setSize (fixedSliderWidth + 20, neededHeight);
    sustainBuiltKey = selectedKey;
}




void SineLabAudioProcessorEditor::releaseSliderChanged (int harmonicIndex)
{
    if (selectedKey != -1)
    {
        int index = audioProcessor.keyStartIndex[selectedKey] + harmonicIndex;
        audioProcessor.oscillators[index].releaseTime = audioProcessor.oscillators[index].setReleaseTime = releaseSliders[harmonicIndex]->getValue();
        repaintGraphArea();
    }
}

void SineLabAudioProcessorEditor::rebuildReleaseSliders()
{
    if (selectedKey != -1 && selectedKey == releaseBuiltKey
        && releaseSliders.size() == audioProcessor.harmonicCounts[selectedKey])
    {
        int startIndex = audioProcessor.keyStartIndex[selectedKey];
        for (int h = 0; h < releaseSliders.size(); ++h)
            releaseSliders[h]->setValue (audioProcessor.oscillators[startIndex + h].releaseTime, juce::dontSendNotification);
        return;
    }

    releaseBuiltKey = -1;
    releaseSliders.clear();
    releaseLabels.clear();
    if (selectedKey == -1)
        return;
    int count = audioProcessor.harmonicCounts[selectedKey];
    int startIndex = audioProcessor.keyStartIndex[selectedKey];
    auto bounds = getLocalBounds();
    int fixedSliderWidth = (bounds.getWidth() * 2 / 3) - 20;
    for (int h = 0; h < count; ++h)
    {
        auto* slider = setupHorizontalHarmonicSlider (releaseContainer, releaseLabels, 0.0, 1.0, 0.0001, h, count, audioProcessor.oscillators[startIndex + h].releaseTime, [this] (int harmonicIndex) { releaseSliderChanged (harmonicIndex); });
        releaseSliders.add (slider);
    }
    int viewportHeight = ((bounds.getHeight() - (bounds.getHeight() / 8)) - ((bounds.getHeight() / 16) + 30));
    int neededHeight = juce::jmax (viewportHeight, count * 30);
    releaseContainer.setSize (fixedSliderWidth + 20, neededHeight);
    releaseBuiltKey = selectedKey;
}



void SineLabAudioProcessorEditor::setupSlider (juce::Slider& slider, juce::Slider::SliderStyle style, double minValue, double maxValue, double interval, std::function<void()> onChange)
{
    addAndMakeVisible (slider);
    slider.setSliderStyle (style);
    slider.setRange (minValue, maxValue, interval);

    if (style == juce::Slider::LinearVertical)
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    else
        slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 20);

    slider.setColour (juce::Slider::thumbColourId, juce::Colours::black);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::black);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::black);
    slider.onValueChange = onChange;
    slider.setVisible (false);
    slider.setLookAndFeel (&thinSliderLookAndFeel);
}


juce::Slider* SineLabAudioProcessorEditor::setupHarmonicSlider (juce::Component& container, juce::OwnedArray<juce::Label>& labelArray, double minValue, double maxValue, double interval, int harmonicIndex, double currentValue, std::function<void(int)> onChange)
{
    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f) + 30;
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int sliderHeight = contentAreaBottom - contentAreaTop;

    auto* slider = new juce::Slider();
    slider->setSliderStyle (juce::Slider::LinearVertical);
    slider->setRange (minValue, maxValue, interval);
    if (minValue < 0.0 && maxValue > 0.0)
            slider->setSkewFactorFromMidPoint (0.0);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    slider->setColour (juce::Slider::thumbColourId, juce::Colours::black);
    slider->setColour (juce::Slider::trackColourId, juce::Colours::black);
    slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::black);
    slider->setLookAndFeel (&thinSliderLookAndFeel);
    slider->onValueChange = [onChange, harmonicIndex] { onChange (harmonicIndex); };
    slider->setValue (currentValue, juce::dontSendNotification);

    container.addAndMakeVisible (slider);
    slider->setBounds (harmonicIndex * 40, 0, 40, sliderHeight);

    auto* label = new juce::Label();
    label->setText (juce::String (harmonicIndex + 1), juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, juce::Colours::black);
    label->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    label->setBounds (harmonicIndex * 40, sliderHeight, 40, 12);

    container.addAndMakeVisible (label);
    labelArray.add (label);

    return slider;
}

juce::Slider* SineLabAudioProcessorEditor::setupHorizontalHarmonicSlider (juce::Component& container, juce::OwnedArray<juce::Label>& labelArray, double minValue, double maxValue, double interval, int harmonicIndex, int harmonicCount, double currentValue, std::function<void(int)> onChange)
{
    auto bounds = getLocalBounds();
    int fixedSliderWidth = (bounds.getWidth() * 2 / 3) - 20;

    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f) + 30;
    int viewportHeight = contentAreaBottom - contentAreaTop;
    int contentHeight = harmonicCount * 30;
    int topPadding = juce::jmax (0, viewportHeight - contentHeight);

    auto* slider = new juce::Slider();
    slider->setSliderStyle (juce::Slider::LinearHorizontal);
    slider->setRange (minValue, maxValue, interval);
    slider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 20);
    slider->setColour (juce::Slider::thumbColourId, juce::Colours::black);
    slider->setColour (juce::Slider::trackColourId, juce::Colours::black);
    slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::black);
    slider->setLookAndFeel (&thinSliderLookAndFeel);
    slider->onValueChange = [onChange, harmonicIndex] { onChange (harmonicIndex); };
    slider->setValue (currentValue, juce::dontSendNotification);

    container.addAndMakeVisible (slider);
    int yPos = topPadding + (harmonicCount - 1 - harmonicIndex) * 30;
    slider->setBounds (26, yPos, fixedSliderWidth, 30);
    auto* label = new juce::Label();
    label->setText (juce::String (harmonicIndex + 1), juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centredLeft);
    label->setColour (juce::Label::textColourId, juce::Colours::black);
    label->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    label->setBounds (0, yPos, 24, 30);

    container.addAndMakeVisible (label);
    labelArray.add (label);

    return slider;
}


void SineLabAudioProcessorEditor::setupGlobalTextBox (ScrollableTextEditor& box, const juce::String& inputChars, int maxChars, std::function<void()> onApply, std::function<void(float)> onScroll)
{
    addAndMakeVisible (box);
    box.setInputRestrictions (maxChars, inputChars);
    box.setJustification (juce::Justification::centred);
    box.setColour (juce::TextEditor::textColourId, juce::Colours::black);
    box.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    box.setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::black);
    box.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    box.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::black);

    box.onReturnKey = [this, &box, onApply]
    {
        onApply();
        unfocusAllComponents();
    };

    box.onFocusLost = onApply;
    box.onScroll = onScroll;
    box.setVisible (false);
}
//==============================================================================
void SineLabAudioProcessorEditor::keyClicked (int noteIndex)
{
    if (selectedKey == noteIndex)
        selectedKey = -1;
    else
        selectedKey = noteIndex;

    if (selectedKey != -1)
    {
        keyVolumeSlider.setValue (audioProcessor.keyVolume[selectedKey], juce::dontSendNotification);
        rebuildSlidersForActiveTab();
    }

    updateControlVisibility();

    repaint();
}

void SineLabAudioProcessorEditor::paintTuningGraph (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f);
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);

    int graphLeft = 0;
    int graphRight = (bounds.getWidth() * 3) / 4;
    int graphTop = contentAreaTop;
    int graphBottom = contentAreaBottom;
    int graphMidY = graphTop + (graphBottom - graphTop) / 2;

    g.setColour (juce::Colours::black);
        g.drawLine ((float) graphLeft, (float) graphTop, (float) graphLeft, (float) graphBottom, 1.0f);
        g.drawLine ((float) graphRight, (float) graphTop, (float) graphRight, (float) graphBottom, 1.0f);
        g.drawLine ((float) graphLeft, (float) graphBottom, (float) graphRight, (float) graphBottom, 1.0f);
        g.drawLine ((float) graphLeft, (float) graphMidY, (float) graphRight, (float) graphMidY, 2.0f);

    if (activeTuningSubTab == 2)
            {
                float fineVerticalSpacingIH = (float) (graphRight - graphLeft) / 88.0f;

                g.setColour (juce::Colours::lightgrey);

                for (int key = 1; key < 88; key += 2)
                {
                    float fineLineX = graphLeft + (fineVerticalSpacingIH * (float) key);
                    g.drawLine (fineLineX, (float) graphTop + 2.0f, fineLineX, (float) graphBottom, 0.5f);
                }

                g.setColour (juce::Colours::lightgrey);
                                        float fineHorizontalSpacingIH = (float) (graphMidY - graphTop) / 15.0f;

                                        for (int step = 1; step <= 14; ++step)
                                        {
                                            float lineYAbove = graphMidY - fineHorizontalSpacingIH * (float) step;
                                            float lineYBelow = graphMidY + fineHorizontalSpacingIH * (float) step;
                                            g.drawLine ((float) graphLeft, lineYAbove, (float) graphRight, lineYAbove, 0.5f);
                                            g.drawLine ((float) graphLeft, lineYBelow, (float) graphRight, lineYBelow, 0.5f);
                                        }

                                        float fullSlotWidthIH = (float) (graphRight - graphLeft) / 88.0f;
                                        float barWidthIH = fullSlotWidthIH / 2.0f;
                                        float barGraphHeightIH = (float) (graphBottom - graphMidY);

                                        g.setColour (juce::Colours::black);

                for (int key = 0; key < 88; ++key)
                                            {
                                                double keyB = juce::jlimit (-0.0015, 0.0015, audioProcessor.keyInharmonicityB[key]);
                                                float barX = graphLeft + (fullSlotWidthIH * (float) key) + (fullSlotWidthIH / 4.0f);
                                                float barHeight = (float) (keyB / 0.0015) * barGraphHeightIH;
                                                if (barHeight >= 0.0f)
                                                    g.fillRect (barX, (float) graphMidY - barHeight, barWidthIH, barHeight);
                                                else
                                                    g.fillRect (barX, (float) graphMidY, barWidthIH, -barHeight);
                                            }

                                            float graphWidthIH = (float) (graphRight - graphLeft);

                                            juce::Array<juce::Point<float>> pathPointsIH;
                                            for (int key = 0; key < 88; ++key)
                                            {
                                                double keyB = juce::jlimit (-0.0015, 0.0015, audioProcessor.keyInharmonicityB[key]);
                                                float x = graphLeft + (graphWidthIH * (float) key / 87.0f);
                                                float y = graphMidY - (float) (keyB / 0.0015) * barGraphHeightIH;
                                                pathPointsIH.add (juce::Point<float> (x, y));
                                            }

                                            g.setColour (juce::Colours::blue);
                                            drawSmoothedPath (g, pathPointsIH);

                                            return;
                                
                            }
    
    
    float fineVerticalSpacing = (float) (graphRight - graphLeft) / 88.0f;
    
    
            g.setColour (juce::Colours::lightgrey);

    for (int key = 1; key < 88; key += 2)
            {
                float fineLineX = graphLeft + (fineVerticalSpacing * (float) key);
                g.drawLine (fineLineX, (float) graphTop + 2.0f, fineLineX, (float) graphBottom, 0.5f);
            }

            float fineHorizontalSpacing = ((float) (graphBottom - graphTop) / 2.0f) / 50.0f;
            g.setColour (juce::Colours::lightgrey);

    for (int cent = -49; cent <= 49; cent += 2)
            {
                if (cent == 0)
                    continue;

                float fineLineY = graphMidY - (fineHorizontalSpacing * (float) cent);
                g.drawLine ((float) graphLeft, fineLineY, (float) graphRight, fineLineY, 0.5f);
            }
    
    float fullSlotWidth = (float) (graphRight - graphLeft) / 88.0f;
        float barWidth = fullSlotWidth / 2.0f;
    
        float barGraphHeight = (float) (graphBottom - graphTop);

        g.setColour (juce::Colours::black);

        for (int key = 0; key < 88; ++key)
        {
            int fundamentalIndex = audioProcessor.keyStartIndex[key];
            double liveTuningValue = audioProcessor.oscillators[fundamentalIndex].stretchCents;
            
            float barX = graphLeft + (fullSlotWidth * (float) key) + (fullSlotWidth / 4.0f);
            double clampedBarValue = juce::jlimit (-50.0, 50.0, liveTuningValue);
            float barY = graphMidY - (float) (clampedBarValue / 50.0) * (barGraphHeight / 2.0f);

            if (clampedBarValue >= 0.0)
                g.fillRect (barX, barY, barWidth, (float) graphMidY - barY);
            else
                g.fillRect (barX, (float) graphMidY, barWidth, barY - (float) graphMidY);
        }

    float graphWidth = (float) (graphRight - graphLeft);
    float graphHeight = (float) (graphBottom - graphTop);

    juce::Array<juce::Point<float>> pathPoints;
    for (int key = 0; key < 88; ++key)
    {
        int fundamentalIndex = audioProcessor.keyStartIndex[key];
        double liveTuningValue = audioProcessor.oscillators[fundamentalIndex].stretchCents;
        float x = graphLeft + (graphWidth * (float) key / 87.0f);
        double clampedValue = juce::jlimit (-50.0, 50.0, liveTuningValue);
        float y = graphMidY - (float) (clampedValue / 50.0) * (graphHeight / 2.0f);
        pathPoints.add (juce::Point<float> (x, y));
    }

    g.setColour (juce::Colours::blue);
    drawSmoothedPath (g, pathPoints);
    
    
}

void SineLabAudioProcessorEditor::paintAttackGraph (juce::Graphics& g)
{
    paintBarGraph (g, [this] (int fi) { return audioProcessor.oscillators[fi].attackTime; }, 0.025);
}

void SineLabAudioProcessorEditor::paintSustainGraph (juce::Graphics& g)
{
    paintBarGraph (g, [this] (int fi) { return audioProcessor.oscillators[fi].sustainLevel; }, 1.0);
}

void SineLabAudioProcessorEditor::paintAmpGraph (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int graphTop    = (int) (bounds.getHeight() / 16.0f);
    int graphBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int graphLeft   = 0;
    int graphRight  = (bounds.getWidth() * 3) / 4;

    g.setColour (juce::Colours::black);
    g.drawLine ((float) graphLeft,  (float) graphTop,    (float) graphLeft,  (float) graphBottom, 1.0f);
    g.drawLine ((float) graphRight, (float) graphTop,    (float) graphRight, (float) graphBottom, 1.0f);
    g.drawLine ((float) graphLeft,  (float) graphBottom, (float) graphRight, (float) graphBottom, 1.0f);

    float usableHeight = (float) (graphBottom - graphTop);
    float spacing = usableHeight / 26.0f;

    g.setColour (juce::Colours::lightgrey);
    for (int step = 1; step <= 25; ++step)
    {
        float lineY = (float) graphTop + spacing * (float) step;
        g.drawLine ((float) graphLeft, lineY, (float) graphRight, lineY, 0.5f);
    }

    float slotWidth = (float) (graphRight - graphLeft) / 44.0f;
    for (int i = 1; i < 44; ++i)
    {
        float lineX = (float) graphLeft + slotWidth * (float) i;
        g.drawLine (lineX, (float) graphTop, lineX, (float) graphBottom, 0.5f);
    }

    float fullSlotWidth = (float) (graphRight - graphLeft) / 88.0f;
    float barWidth = fullSlotWidth / 2.0f;
    float graphWidth = (float) (graphRight - graphLeft);

    g.setColour (juce::Colours::black);

    for (int key = 0; key < 88; ++key)
    {
        double dc = audioProcessor.keyDutyCycle[key];
        float barHeight = (float) (dc / 0.5) * usableHeight;
        float barX = (float) graphLeft + fullSlotWidth * (float) key + (fullSlotWidth / 4.0f);
        float barY = (float) graphBottom - barHeight;
        g.fillRect (barX, barY, barWidth, barHeight);
    }

    juce::Array<juce::Point<float>> pathPoints;
    for (int key = 0; key < 88; ++key)
    {
        double dc = audioProcessor.keyDutyCycle[key];
        float x = (float) graphLeft + graphWidth * (float) key / 87.0f;
        float y = (float) graphBottom - (float) (dc / 0.5) * usableHeight;
        pathPoints.add (juce::Point<float> (x, y));
    }

    g.setColour (juce::Colours::blue);
    drawSmoothedPath (g, pathPoints);
}

void SineLabAudioProcessorEditor::paintKeyVolumeGraph (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int graphTop    = (int) (bounds.getHeight() / 16.0f);
    int graphBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int graphLeft   = 0;
    int graphRight  = (bounds.getWidth() * 3) / 4;

    g.setColour (juce::Colours::black);
    g.drawLine ((float) graphLeft,  (float) graphTop,    (float) graphLeft,  (float) graphBottom, 1.0f);
    g.drawLine ((float) graphRight, (float) graphTop,    (float) graphRight, (float) graphBottom, 1.0f);
    g.drawLine ((float) graphLeft,  (float) graphBottom, (float) graphRight, (float) graphBottom, 1.0f);

    float usableHeight = (float) (graphBottom - graphTop);
    float spacing = usableHeight / 10.0f;

    g.setColour (juce::Colours::lightgrey);
    for (int step = 1; step <= 9; ++step)
    {
        float lineY = (float) graphTop + spacing * (float) step;
        g.drawLine ((float) graphLeft, lineY, (float) graphRight, lineY, 0.5f);
    }

    float slotWidth = (float) (graphRight - graphLeft) / 44.0f;
    for (int i = 1; i < 44; ++i)
    {
        float lineX = (float) graphLeft + slotWidth * (float) i;
        g.drawLine (lineX, (float) graphTop, lineX, (float) graphBottom, 0.5f);
    }

    float fullSlotWidth = (float) (graphRight - graphLeft) / 88.0f;
    float barWidth = fullSlotWidth / 2.0f;

    g.setColour (juce::Colours::black);

    for (int key = 0; key < 88; ++key)
    {
        float v       = (float) juce::jlimit (0.0, 1.0, audioProcessor.keyVolume[key]);
        float barH    = v * usableHeight;
        float barX    = (float) graphLeft + fullSlotWidth * (float) key + (fullSlotWidth / 4.0f);
        float barY    = (float) graphBottom - barH;
        g.fillRect (barX, barY, barWidth, barH);
    }
}

void SineLabAudioProcessorEditor::paintEvenMorphGraph (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int graphTop    = (int) (bounds.getHeight() / 16.0f);
    int graphBottom = (int) (bounds.getHeight() - keyboardAreaHeight);
    int graphLeft   = 0;
    int graphRight  = (bounds.getWidth() * 3) / 4;
    float usableHeight = (float) (graphBottom - graphTop);
    float graphWidth   = (float) (graphRight - graphLeft);

    // grid
    g.setColour (juce::Colours::lightgrey);
    for (int step = 1; step <= 9; ++step)
    {
        float lineY = (float) graphTop + (usableHeight / 10.0f) * (float) step;
        g.drawLine ((float) graphLeft, lineY, (float) graphRight, lineY, 0.5f);
    }
    float slotWidth = graphWidth / 44.0f;
    for (int i = 1; i < 44; ++i)
    {
        float lineX = (float) graphLeft + slotWidth * (float) i;
        g.drawLine (lineX, (float) graphTop, lineX, (float) graphBottom, 0.5f);
    }

    // border
    g.setColour (juce::Colours::black);
    g.drawLine ((float) graphLeft,  (float) graphTop,    (float) graphLeft,  (float) graphBottom, 1.0f);
    g.drawLine ((float) graphRight, (float) graphTop,    (float) graphRight, (float) graphBottom, 1.0f);
    g.drawLine ((float) graphLeft,  (float) graphBottom, (float) graphRight, (float) graphBottom, 1.0f);

    // bars
    float fullSlotWidth = graphWidth / 88.0f;
    float barWidth = fullSlotWidth / 2.0f;
    g.setColour (juce::Colours::black);
    for (int key = 0; key < 88; ++key)
    {
        float v    = (float) juce::jlimit (0.0, 1.0, audioProcessor.evenMorphStrength[key]);
        float barH = v * usableHeight;
        float barX = (float) graphLeft + fullSlotWidth * (float) key + (fullSlotWidth / 4.0f);
        float barY = (float) graphBottom - barH;
        g.fillRect (barX, barY, barWidth, barH);
    }

    // smoothed line
    juce::Array<juce::Point<float>> pathPoints;
    for (int key = 0; key < 88; ++key)
    {
        float v = (float) juce::jlimit (0.0, 1.0, audioProcessor.evenMorphStrength[key]);
        float x = (float) graphLeft + graphWidth * (float) key / 87.0f;
        float y = (float) graphBottom - v * usableHeight;
        pathPoints.add ({ x, y });
    }
    g.setColour (juce::Colours::blue);
    drawSmoothedPath (g, pathPoints);
}

void SineLabAudioProcessorEditor::paintToggleGraph (juce::Graphics& g, int componentWidth, int componentHeight)
{
    double scale = (double) getWidth() / 792.0;
    float colWidth  = (float) componentWidth / 88.0f;
    float graphH    = (float) componentHeight;
    float gridStart = 2.0f;

    g.setColour (juce::Colours::black);
    g.drawLine (0.0f, 0.0f, (float) componentWidth, 0.0f, 1.0f);

    const float slotH = 10.0f * (float)scale;

    // Only draw what intersects the visible scroll region — identical pixels,
    // far less work than iterating all 12,870 cells on every paint
    auto clip = g.getClipBounds();
    int firstRow = juce::jmax (1, (int) (clip.getY() / slotH) - 1);
    int lastRow  = (int) (clip.getBottom() / slotH) + 1;
    int firstKey = juce::jmax (0,  (int) (clip.getX() / colWidth) - 1);
    int lastKey  = juce::jmin (87, (int) (clip.getRight() / colWidth) + 1);

    g.setColour (juce::Colours::lightgrey);
    for (int k = juce::jmax (1, firstKey); k <= lastKey; ++k)
        g.drawLine (k * colWidth, gridStart, k * colWidth, graphH, 0.5f);

    g.setColour (juce::Colours::lightgrey);
    for (int row = firstRow; row <= lastRow && row * slotH < graphH; ++row)
        g.drawLine (0.0f, row * slotH, (float) componentWidth, row * slotH, 0.5f);

    // cell for harmonic i sits at row 726 - i
    int firstHarm = juce::jmax (0, 726 - lastRow);
    int lastHarm  = 726 - firstRow + 1;

    for (int key = firstKey; key <= lastKey; ++key)
    {
        int   base    = audioProcessor.keyStartIndex[key];
        int   count   = audioProcessor.harmonicCounts[key];
        float colX    = key * colWidth;
        for (int i = firstHarm; i < count && i <= lastHarm; ++i)
        {
            auto& osc = audioProcessor.oscillators[base + i];
            if (! osc.isAudible())
                continue;

            int   row = 726 - i;
            float y   = (float) row * slotH;
            g.setColour (juce::Colours::black);
            g.drawRect (colX, y, colWidth, slotH, 1.0f);
        }
    }

}

void SineLabAudioProcessorEditor::paintBarGraph (juce::Graphics& g, std::function<double(int)> valueGetter, double yMax, bool drawTrendline)
{
    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f);
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);

    int graphLeft = 0;
    int graphRight = (bounds.getWidth() * 3) / 4;
    int graphTop = contentAreaTop;
    int graphBottom = contentAreaBottom;

    g.setColour (juce::Colours::black);
    g.drawLine ((float) graphLeft, (float) graphTop, (float) graphLeft, (float) graphBottom, 1.0f);
    g.drawLine ((float) graphRight, (float) graphTop, (float) graphRight, (float) graphBottom, 1.0f);
    g.drawLine ((float) graphLeft, (float) graphBottom, (float) graphRight, (float) graphBottom, 1.0f);

    float fineVerticalSpacing = (float) (graphRight - graphLeft) / 88.0f;

    g.setColour (juce::Colours::lightgrey);

    for (int key = 1; key < 88; key += 2)
    {
        float fineLineX = graphLeft + (fineVerticalSpacing * (float) key);
        g.drawLine (fineLineX, (float) graphTop + 2.0f, fineLineX, (float) graphBottom, 0.5f);
    }

    float usableHeight = (float) (graphBottom - graphTop - 2);
    float fineHorizontalSpacing = usableHeight / 20.0f;

    for (int step = 1; step <= 19; ++step)
    {
        float lineY = graphTop + 2.0f + fineHorizontalSpacing * (float) step;
        g.drawLine ((float) graphLeft, lineY, (float) graphRight, lineY, 0.5f);
    }

    float fullSlotWidth = (float) (graphRight - graphLeft) / 88.0f;
    float barWidth = fullSlotWidth / 2.0f;
    float barGraphHeight = (float) (graphBottom - graphTop);
    float graphWidth = (float) (graphRight - graphLeft);

    g.setColour (juce::Colours::black);

    for (int key = 0; key < 88; ++key)
    {
        int fi = audioProcessor.keyStartIndex[key];
        double value = juce::jlimit (0.0, yMax, valueGetter (fi));

        float barX = graphLeft + (fullSlotWidth * (float) key) + (fullSlotWidth / 4.0f);
        float barHeight = (float) (value / yMax) * barGraphHeight;
        float barY = (float) graphBottom - barHeight;

        g.fillRect (barX, barY, barWidth, barHeight);
    }

    if (drawTrendline)
    {
        juce::Array<juce::Point<float>> pathPoints;
        for (int key = 0; key < 88; ++key)
        {
            int fi = audioProcessor.keyStartIndex[key];
            double value = juce::jlimit (0.0, yMax, valueGetter (fi));
            float x = graphLeft + (graphWidth * (float) key / 87.0f);
            float y = (float) graphBottom - (float) (value / yMax) * barGraphHeight;
            pathPoints.add (juce::Point<float> (x, y));
        }

        g.setColour (juce::Colours::blue);
        drawSmoothedPath (g, pathPoints);
    }
}

void SineLabAudioProcessorEditor::paintDecayGraph (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    float keyboardAreaHeight = bounds.getHeight() / 8.0f;
    int contentAreaTop = (int) (bounds.getHeight() / 16.0f);
    int contentAreaBottom = (int) (bounds.getHeight() - keyboardAreaHeight);

    int graphLeft = 0;
    int graphRight = (bounds.getWidth() * 3) / 4;
    int graphTop = contentAreaTop;
    int graphBottom = contentAreaBottom;

    float barGraphHeight = (float) (graphBottom - graphTop);
    float graphWidth = (float) (graphRight - graphLeft);

    g.setColour (juce::Colours::black);
    g.drawLine ((float) graphLeft, (float) graphTop, (float) graphLeft, (float) graphBottom, 1.0f);
    g.drawLine ((float) graphRight, (float) graphTop, (float) graphRight, (float) graphBottom, 1.0f);
    g.drawLine ((float) graphLeft, (float) graphBottom, (float) graphRight, (float) graphBottom, 1.0f);

    float usableHeight = (float) (graphBottom - graphTop - 2);

    g.setColour (juce::Colours::lightgrey);

    float fineVerticalSpacing = graphWidth / 88.0f;
    for (int key = 1; key < 88; key += 2)
    {
        float fineLineX = graphLeft + (fineVerticalSpacing * (float) key);
        g.drawLine (fineLineX, (float) graphTop + 2.0f, fineLineX, (float) graphBottom, 0.5f);
    }

    float fineHorizontalSpacing = usableHeight / 20.0f;
    for (int step = 1; step <= 19; ++step)
    {
        float lineY = graphTop + 2.0f + fineHorizontalSpacing * (float) step;
        g.drawLine ((float) graphLeft, lineY, (float) graphRight, lineY, 0.5f);
    }

    if (activeDecaySubTab == 2)
    {
        // Key 0 (A0) has the most harmonics; all keys share the same shape ratios
        int startIndex = audioProcessor.keyStartIndex[0];
        int bestCount = audioProcessor.harmonicCounts[0];

        if (bestCount >= 2)
        {
            // x = decayShapeRatio (0..1 → left..right), y = harmonic index (0=bottom, last=top)
            juce::Array<juce::Point<float>> pts;
            for (int h = 0; h < bestCount; ++h)
            {
                float x = (float) graphLeft + (float) audioProcessor.oscillators[startIndex + h].decayShapeRatio * graphWidth;
                float y = (float) graphBottom - ((float) h / (float) (bestCount - 1)) * barGraphHeight;
                pts.add ({ x, y });
            }

            g.setColour (juce::Colours::blue);
            drawSmoothedPath (g, pts);
        }
        return;
    }

    paintBarGraph (g, [this] (int fi) { return audioProcessor.oscillators[fi].decayTime; }, 10.0);
}

void SineLabAudioProcessorEditor::paintReleaseGraph (juce::Graphics& g)
{
    paintBarGraph (g, [this] (int fi) { return audioProcessor.oscillators[fi].releaseTime; }, 0.025);
}

void SineLabAudioProcessorEditor::rebuildSlidersForActiveTab()
{
    switch (activeTab)
    {
        case 0: rebuildToggleButtons();    toggleViewport.setViewPosition   (0, toggleContainer.getHeight());    break;
        case 1: rebuildAmplitudeSliders(); amplitudeViewport.setViewPosition (0, 0);                             break;
        case 2: rebuildTuningSliders();    tuningViewport.setViewPosition    (0, 0);                             break;
        case 3: rebuildPhaseSliders();     phaseViewport.setViewPosition     (0, 0);                             break;
        case 4: rebuildPanSliders();       panViewport.setViewPosition       (0, panContainer.getHeight());      break;
        case 5: rebuildAttackSliders();    attackViewport.setViewPosition    (0, attackContainer.getHeight());   break;
        case 6: rebuildDecaySliders();     decayViewport.setViewPosition     (0, decayContainer.getHeight());    break;
        case 7: rebuildSustainSliders();   sustainViewport.setViewPosition   (0, sustainContainer.getHeight());  break;
        case 8: rebuildReleaseSliders();   releaseViewport.setViewPosition   (0, releaseContainer.getHeight());  break;
        default: break;
    }
}

juce::File SineLabAudioProcessorEditor::getPresetsFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("sineLab Presets");
    if (! folder.exists())
        folder.createDirectory();
    return folder;
}

void SineLabAudioProcessorEditor::refreshPresetList()
{
    presetBrowserComboBox.clear (juce::dontSendNotification);
    presetBrowserComboBox.addItem ("(no preset)", 1);

    auto files = getPresetsFolder().findChildFiles (juce::File::findFiles, false, "*.sinelab");
    files.sort();

    for (int i = 0; i < files.size(); ++i)
        presetBrowserComboBox.addItem (files[i].getFileNameWithoutExtension(), i + 2);

    presetBrowserComboBox.setSelectedId (1, juce::dontSendNotification);
}

void SineLabAudioProcessorEditor::savePreset()
{
    auto* aw = new juce::AlertWindow ("Save Preset", "Enter a name for this preset:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "New Preset");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        if (result == 1)
        {
            auto name = aw->getTextEditorContents ("name").trim();
            if (name.isNotEmpty())
            {
                juce::MemoryBlock state;
                audioProcessor.getStateInformation (state);
                getPresetsFolder().getChildFile (name + ".sinelab")
                    .replaceWithData (state.getData(), state.getSize());
                refreshPresetList();

                for (int i = 2; i <= presetBrowserComboBox.getNumItems() + 1; ++i)
                {
                    if (presetBrowserComboBox.getItemText (i - 1) == name)
                    {
                        presetBrowserComboBox.setSelectedId (i, juce::dontSendNotification);
                        break;
                    }
                }
            }
        }
        delete aw;
    }), true);
}

void SineLabAudioProcessorEditor::loadPreset (int comboBoxId)
{
    if (comboBoxId <= 1)
        return;

    auto files = getPresetsFolder().findChildFiles (juce::File::findFiles, false, "*.sinelab");
    files.sort();

    int fileIndex = comboBoxId - 2;
    if (fileIndex < 0 || fileIndex >= files.size())
        return;

    juce::MemoryBlock state;
    if (files[fileIndex].loadFileAsData (state))
    {
        // setStateInformation broadcasts a change message; changeListenerCallback
        // then runs tabSelected once — no second rebuild here
        audioProcessor.setStateInformation (state.getData(), (int) state.getSize());
    }
}

void SineLabAudioProcessorEditor::deletePreset()
{
    int id = presetBrowserComboBox.getSelectedId();
    if (id <= 1)
        return;

    auto files = getPresetsFolder().findChildFiles (juce::File::findFiles, false, "*.sinelab");
    files.sort();

    int fileIndex = id - 2;
    if (fileIndex < 0 || fileIndex >= files.size())
        return;

    auto name = files[fileIndex].getFileNameWithoutExtension();

    auto* aw = new juce::AlertWindow ("Delete Preset", "", juce::MessageBoxIconType::NoIcon);
    aw->addButton ("Yes", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("No",  0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, files, fileIndex, aw] (int result)
    {
        if (result == 1)
        {
            files[fileIndex].deleteFile();
            refreshPresetList();
        }
        delete aw;
    }), true);
}

void SineLabAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (dynamic_cast<juce::ResizableCornerComponent*> (e.eventComponent) != nullptr)
    {
        setSize (792, 500);
    }
}

void SineLabAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    unfocusAllComponents();
}
