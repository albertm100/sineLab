/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
struct OscillatorState
{
    bool active = false;
    double amplitude = 1.0;
    double startPhase = 0.0;
    bool manuallyMuted = false;
    bool aboveCeiling  = false;

    float rotationCos = 1.0f;
        float rotationSin = 0.0f;
        float deltaCos = 1.0f;
        float deltaSin = 0.0f;
    
    int tuningCents = 0;
    int stretchCents = 0;
    int inharmonicityCents = 0;
    void recombineTuningCents() { tuningCents = stretchCents + inharmonicityCents; }

    int maxUpwardCents = 0;
    int maxDownwardCents = 0;
    int audibleMaxCents = 0;
    int midiNoteNumber = 0;
    int harmonicNumber = 1;
    int ownerKey = 0;
    bool needsFrequencyUpdate = true;
    double decayShapeRatio = 1.0;

    int startPhaseDegrees = 0;
    int setStartPhaseDegrees = 0;
    bool needsPhaseUpdate = false;
    double pan = 0.0;
    bool needsPanUpdate = true;
    float leftGain = 1.0f;
    float rightGain = 1.0f;
    
    float attackTime = 0.0001f;
    float setAttackTime = 0.0001f;
    float decayTime = 0.0001f;
    float sustainLevel = 1.0f;
    float envelopeElapsed = 0.0f;
    float envelopeValue = 0.0f;

    bool isReleasing = false;
    bool inAttack = true;
    float releaseElapsed = 0.0f;
    float levelAtReleaseStart = 0.0f;
    float releaseTime = 0.0001f;
    float setReleaseTime = 0.0001f;

    int activeRank  = 0;
    int activeCount = 1;
};

//==============================================================================
/**
*/
class SineLabAudioProcessor  : public juce::AudioProcessor,
                               public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    SineLabAudioProcessor();
    ~SineLabAudioProcessor() override;
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateActiveRanks();
    //==============================================================================
    std::vector<OscillatorState> oscillators;
    std::vector<int> harmonicCounts;
    std::vector<int> keyStartIndex;
    std::vector<double> keyVolume;
    std::vector<double> keyDutyCycle;
    
    
    int lastAppliedStretchA0 = 0;
    int lastAppliedStretchC8 = 0;

    double lastAppliedAmpA0 = 0.25;
    double lastAppliedAmpC8 = 0.25;
    int lastAppliedAmpStartKey = 0;
    int lastAppliedAmpEndKey = 87;

    double lastAppliedKeyVolumeA0  = 1.0;
    double lastAppliedKeyVolumeC8  = 1.0;
    int    lastAppliedKeyVolumeStartKey = 0;
    int    lastAppliedKeyVolumeEndKey   = 87;
    int    lastAppliedExpKKeyVolume     = 4;
    double globalKeyVolumeValue         = 1.0;
    int    lastAppliedExpKEvenMorph      = 4;
    int    lastAppliedEvenMorphStartKey  = 0;
    int    lastAppliedEvenMorphEndKey    = 87;
    double lastAppliedEvenMorphA0        = 1.0;
    double lastAppliedEvenMorphC8        = 0.0;
    double evenMorphStrength[88]         = {};

    double lastAppliedAttackA0 = 0.0001;
    double lastAppliedAttackC8 = 0.0001;
    double lastAppliedAttackRand = 0.0;
    double lastAppliedReleaseRand = 0.0;
    double lastAppliedDecayA0 = 0.0;
    double lastAppliedDecayC8 = 0.0;
    double lastAppliedDecayIIThreshold = 0.0;
    double lastAppliedReleaseA0 = 0.0;
    double lastAppliedReleaseC8 = 0.0;

    int lastAppliedToggleA0 = 1;
    int lastAppliedToggleC8 = 1;
    int lastAppliedPhaseA0 = 1;
    int lastAppliedPhaseC8 = 1;
    double lastAppliedPhaseRand = 0.0;
    int lastAppliedPhaseEvens = 1;
    int lastAppliedExpKToggle = 4;
    int lastAppliedExpKAmp = 4;
    int lastAppliedExpKTuning1 = 4;
    int lastAppliedExpKTuning2 = 4;
    int lastAppliedExpKAttack = 4;
    int lastAppliedExpKDecay1 = 4;
    int lastAppliedExpKDecay2 = 4;
    int lastAppliedExpKRelease = 4;
    int lastAppliedExpKPhase = 4;
    
    double lastAppliedInharmonicityA0 = 0.0;
            double lastAppliedInharmonicityC8 = 0.0;
        std::vector<double> keyInharmonicityB;
    
    
    
    
    std::vector<double> keyVelocityScalar;

    
    bool normalizationEnabled = false;
    bool taperCTEnabled  = false;
    bool taperACTEnabled = false;
    bool globalToggleState       = true;
    bool firstHarmonicToggleState = true;
    bool evensToggleState         = true;
    bool primesToggleState        = true;
    bool everyNToggleState        = true;
    int  everyNValue              = 3;
    double globalAmpValue = 1.0;
    int globalTuningValue = 0;
    int globalPhaseValue = 0;
    double globalPanValue = 0.0;
    double lastAppliedPanWidth = 1.0;
    double globalAttackValue = 0.0;
    double globalSustainValue = 1.0;
    int    lastAppliedSustainStartKey = 0;
    double lastAppliedSustainA0       = 1.0;
    int    lastAppliedSustainEndKey   = 87;
    double lastAppliedSustainC8       = 1.0;
    int    lastAppliedExpKSustain     = 0;
    double globalDecayValue = 0.0001;
    double globalReleaseValue = 0.0001;
    
private:
    double currentSampleRate = 44100.0;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SineLabAudioProcessor)
};
