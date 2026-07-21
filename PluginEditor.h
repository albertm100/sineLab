/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
//==============================================================================
class ThinSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    float trackThickness = 2.0f;
    bool  fixedThumbRadius = false;

    int getSliderThumbRadius (juce::Slider& s) override
    {
        return fixedThumbRadius ? 5 : juce::LookAndFeel_V4::getSliderThumbRadius (s);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {

        if (style == juce::Slider::LinearVertical)
        {
            float trackX = x + width / 2.0f;
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.drawLine (trackX, (float) y, trackX, (float) (y + height), trackThickness);
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillEllipse (trackX - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f);
        }
        else
        {
            float trackY = y + height / 2.0f;
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.drawLine ((float) x, trackY, (float) (x + width), trackY, trackThickness);
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillEllipse (sliderPos - 5.0f, trackY - 5.0f, 10.0f, 10.0f);
        }
    }
};

class ScrollableTextEditor : public juce::TextEditor
{
public:
    std::function<void(float)> onScroll;

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (onScroll)
            onScroll (wheel.deltaY);
    }
};

class InvisibleKeyButton : public juce::Button
{
public:
    InvisibleKeyButton() : juce::Button ({}) {}
    void paintButton (juce::Graphics&, bool, bool) override
    {
    }

    std::function<void(bool)> onHoverChange;

    void mouseEnter (const juce::MouseEvent&) override
    {
        if (onHoverChange)
            onHoverChange (true);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (onHoverChange)
            onHoverChange (false);
    }
};

//==============================================================================
class SineLabAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                     public juce::ChangeListener
{
public:
    SineLabAudioProcessorEditor (SineLabAudioProcessor&);
    ~SineLabAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void paintToggleGraph (juce::Graphics& g, int componentWidth, int componentHeight);


private:
    struct ToggleGraphComponent : public juce::Component
    {
        SineLabAudioProcessorEditor& owner;
        ToggleGraphComponent (SineLabAudioProcessorEditor& o) : owner (o) {}
        void paint (juce::Graphics& g) override
        {
            owner.paintToggleGraph (g, getWidth(), getHeight());
        }
    };
    ToggleGraphComponent toggleGraphComponent { *this };
    juce::Viewport        toggleGraphViewport;
    SineLabAudioProcessor& audioProcessor;
    ThinSliderLookAndFeel thinSliderLookAndFeel;
    ThinSliderLookAndFeel ampSubTabSliderLookAndFeel;
    ThinSliderLookAndFeel tuningSubTabSliderLookAndFeel;
    ThinSliderLookAndFeel decaySubTabSliderLookAndFeel;
    juce::OwnedArray<InvisibleKeyButton> keyButtons;
    int selectedKey = -1;
    int hoveredKey = -1;
    int activeTab = 0;
    int activeAmpSubTab = 1;
    int activeTuningSubTab = 1;
    int activeDecaySubTab = 1;

    void keyClicked (int noteIndex);
    void tabSelected (int tabIndex);
    void rebuildSlidersForActiveTab();
    
    void updateControlVisibility();
    
    void paintTuningGraph (juce::Graphics& g);
    void paintAttackGraph (juce::Graphics& g);
    void paintSustainGraph (juce::Graphics& g);
    void applyShapeToSustain (std::function<double(double)> shapeFunction);
    void paintDecayGraph (juce::Graphics& g);
    void paintReleaseGraph (juce::Graphics& g);
    void paintAmpGraph (juce::Graphics& g);
    void paintKeyVolumeGraph (juce::Graphics& g);
    void paintEvenMorphGraph (juce::Graphics& g);
    void paintBarGraph (juce::Graphics& g, std::function<double(int)> valueGetter, double yMax, bool drawTrendline = true);
    
    void setupSlider (juce::Slider& slider, juce::Slider::SliderStyle style, double minValue, double maxValue, double interval, std::function<void()> onChange);
    
    
    juce::Slider* setupHarmonicSlider (juce::Component& container, juce::OwnedArray<juce::Label>& labelArray, double minValue, double maxValue, double interval, int harmonicIndex, double currentValue, std::function<void(int)> onChange);
    juce::Slider* setupHorizontalHarmonicSlider (juce::Component& container, juce::OwnedArray<juce::Label>& labelArray, double minValue, double maxValue, double interval, int harmonicIndex, int harmonicCount, double currentValue, std::function<void(int)> onChange);
    
    
    void setupGlobalTextBox (ScrollableTextEditor& box, const juce::String& inputChars, int maxChars, std::function<void()> onApply, std::function<void(float)> onScroll);

    
    
    InvisibleKeyButton toggleTabButton;
    juce::Viewport toggleViewport;
    juce::Component toggleContainer;
    juce::OwnedArray<juce::ToggleButton> toggleButtons;
    juce::OwnedArray<juce::Label> toggleLabels;
    void rebuildToggleButtons();
    void toggleButtonChanged (int harmonicIndex);
    
    

    InvisibleKeyButton ampTabButton;
    juce::Slider ampSubTabSlider;
    juce::Viewport amplitudeViewport;
    juce::Component amplitudeContainer;
    juce::OwnedArray<juce::Slider> amplitudeSliders;
    juce::OwnedArray<juce::Label> amplitudeLabels;
    void rebuildAmplitudeSliders();
    void amplitudeSliderChanged (int harmonicIndex);
    
    juce::Slider keyVolumeSlider;
    void keyVolumeChanged();
    
    ScrollableTextEditor ampA0TextBox;
    void ampA0Applied();
    ScrollableTextEditor ampC8TextBox;
    void ampC8Applied();
    juce::TextEditor ampA0UpperTextBox;
    juce::TextEditor ampC8UpperTextBox;
    juce::TextEditor evenMorphA0UpperTextBox;
    juce::TextEditor evenMorphC8UpperTextBox;

    juce::ToggleButton normalizationCheckbox;
    void normalizationToggled();

    juce::ToggleButton taperCTCheckbox;
    juce::ToggleButton taperACTCheckbox;
    void taperCTToggled();
    void taperACTToggled();
    void recalculateKeyVolume (int key);
    void recalculateAllKeyVolumes();
    void reapplyDecayShapeForKey (int key);
    
    juce::TextButton applyOneOverSqrtNButton;
    void applyOneOverSqrtNClicked();

    juce::TextButton applyOneOverNButton;
    void applyOneOverNClicked();

    juce::TextButton applyOneOverNSquaredButton;
        void applyOneOverNSquaredClicked();
    

    InvisibleKeyButton tuningTabButton;
    juce::Viewport tuningViewport;
    juce::Component tuningContainer;
    juce::OwnedArray<juce::Slider> tuningSliders;
    juce::OwnedArray<juce::Label> tuningLabels;
    void rebuildTuningSliders();
    void tuningSliderChanged (int harmonicIndex);
    
    InvisibleKeyButton phaseTabButton;
    juce::Viewport phaseViewport;
    juce::Component phaseContainer;
    juce::OwnedArray<juce::Slider> phaseSliders;
    juce::OwnedArray<juce::Label> phaseLabels;
    void rebuildPhaseSliders();
    void phaseSliderChanged (int harmonicIndex);
    
    
    InvisibleKeyButton panTabButton;
    juce::Viewport panViewport;
    juce::Component panContainer;
    juce::OwnedArray<juce::Slider> panSliders;
    juce::OwnedArray<juce::Label> panLabels;
    void rebuildPanSliders();
    void panSliderChanged (int harmonicIndex);
    
    

    InvisibleKeyButton attackTabButton;
    juce::Viewport attackViewport;
    juce::Component attackContainer;
    juce::OwnedArray<juce::Slider> attackSliders;
    juce::OwnedArray<juce::Label> attackLabels;
    void rebuildAttackSliders();
    void attackSliderChanged (int harmonicIndex);
    
    
    
    InvisibleKeyButton decayTabButton;
    juce::Viewport decayViewport;
    juce::Component decayContainer;
    juce::OwnedArray<juce::Slider> decaySliders;
    juce::OwnedArray<juce::Label> decayLabels;
    void rebuildDecaySliders();
    void decaySliderChanged (int harmonicIndex);
    
    InvisibleKeyButton sustainTabButton;
    juce::Viewport sustainViewport;
    juce::Component sustainContainer;
    juce::OwnedArray<juce::Slider> sustainSliders;
    juce::OwnedArray<juce::Label> sustainLabels;
    void rebuildSustainSliders();
    void sustainSliderChanged (int harmonicIndex);
    
    
    
    InvisibleKeyButton releaseTabButton;
    juce::Viewport releaseViewport;
    juce::Component releaseContainer;
    juce::OwnedArray<juce::Slider> releaseSliders;
    juce::OwnedArray<juce::Label> releaseLabels;
    void rebuildReleaseSliders();
    void releaseSliderChanged (int harmonicIndex);
    
    
    
    
    juce::ToggleButton globalToggleCheckbox;
    void globalToggleChanged();

    juce::ToggleButton firstHarmonicToggleCheckbox;
    juce::Label firstHarmonicLabel;
    void firstHarmonicToggleChanged();
    
    juce::ToggleButton evensToggleCheckbox;
    juce::Label evensLabel;
    void evensToggleChanged();

    juce::ToggleButton primesToggleCheckbox;
    juce::Label primesLabel;
    void primesToggleChanged();

    juce::ToggleButton  everyNToggleCheckbox;
    ScrollableTextEditor everyNTextBox;
    void everyNToggleChanged();

    ScrollableTextEditor globalAmpTextBox;
    void globalAmpApplied();
    ScrollableTextEditor globalTuningTextBox;
    void globalTuningApplied();
    
    ScrollableTextEditor stretchA0TextBox;
        void stretchA0Applied();
    
    ScrollableTextEditor stretchC8TextBox;
        void stretchC8Applied();
    
    juce::TextButton linearShapeButton;

        
        juce::TextButton cubicShapeButton;

        
        juce::TextButton squaredShapeButton;

        
        juce::TextButton absValueShapeButton;

    
    
    
    juce::TextButton expShapeButton;
    ScrollableTextEditor expSteepnessTextBox;
    
        void expSteepnessApplied();
    
        juce::Slider tuningSubTabSlider;

        void applyShapeToToggle (std::function<double(double)> shapeFunction);
        void applyShapeToAmp (std::function<double(double)> shapeFunction);
        void applyShapeToKeyVolume (std::function<double(double)> shapeFunction);
        void applyShapeToEvenMorph (std::function<double(double)> shapeFunction);
        void applyShapeToStretch (std::function<double(double)> shapeFunction);
        void applyShapeToInharmonicity (std::function<double(double)> shapeFunction);
        void applyShapeToPhase (std::function<double(double)> shapeFunction);
        void applyShapeToAttack (std::function<double(double)> shapeFunction);
        void applyShapeToDecay (std::function<double(double)> shapeFunction);
        void applyShapeToDecayHarmonics (std::function<double(double)> shapeFunction);
        void applyShapeToRelease (std::function<double(double)> shapeFunction);
        void applyShapeToField (std::function<double(double)> shapeFunction, double a0, double c8,
                                double minVal, double maxVal,
                                std::function<void(OscillatorState&, double)> setter,
                                std::function<void()> rebuildFn);
        void wireShapeButtons();
    
    
    ScrollableTextEditor globalPhaseTextBox;
    void globalPhaseApplied();
    ScrollableTextEditor phaseA0TextBox;
    ScrollableTextEditor phaseC8TextBox;
    juce::TextEditor phaseEvensTextBox;
    juce::Label phaseEvensLabel;
    juce::TextEditor phaseRandTextBox;
    juce::Label phaseRandLabel;
    ScrollableTextEditor globalPanTextBox;
    void globalPanApplied();
    ScrollableTextEditor panWidthTextBox;
    juce::TextButton evenLeftOddRightButton;
    void evenLeftOddRightClicked();
    juce::TextButton twoLeftButton;
    void twoLeftClicked();
    juce::TextButton oddLeftEvenRightButton;
    void oddLeftEvenRightClicked();
    juce::TextButton alternateActiveButton;
        void alternateActiveClicked();
    ScrollableTextEditor globalAttackTextBox;
    void globalAttackApplied();
    juce::TextEditor attackRandTextBox;
    juce::Label attackRandLabel;
    juce::TextEditor releaseRandTextBox;
    juce::Label releaseRandLabel;
    ScrollableTextEditor attackA0TextBox;
    void attackA0Applied();
    ScrollableTextEditor attackC8TextBox;
    void attackC8Applied();
    
    
    ScrollableTextEditor globalSustainTextBox;
    void globalSustainApplied();
    juce::TextEditor     sustainA0UpperTextBox;
    ScrollableTextEditor sustainA0TextBox;
    juce::TextEditor     sustainC8UpperTextBox;
    ScrollableTextEditor sustainC8TextBox;
    juce::TextButton     sustainLinearButton;
    juce::TextButton     sustainSquaredButton;
    juce::TextButton     sustainCubicButton;
    juce::TextButton     sustainAbsValueButton;
    juce::TextButton     sustainExpButton;
    ScrollableTextEditor sustainExpKTextBox;
    ScrollableTextEditor globalDecayTextBox;
    void globalDecayApplied();
    ScrollableTextEditor decayA0TextBox;
    void decayA0Applied();
    ScrollableTextEditor decayC8TextBox;
    void decayC8Applied();
    ScrollableTextEditor decayIIA0TextBox;
    std::function<double(double)> lastDecayIIShape { [] (double t) { return (t + 1.0) / 2.0; } };
    juce::Slider decaySubTabSlider;
    ScrollableTextEditor globalReleaseTextBox;
    void globalReleaseApplied();
    ScrollableTextEditor releaseA0TextBox;
    void releaseA0Applied();
    ScrollableTextEditor releaseC8TextBox;
    void releaseC8Applied();
    
    ScrollableTextEditor toggleA0TextBox;
    ScrollableTextEditor toggleC8TextBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SineLabAudioProcessorEditor)
};
