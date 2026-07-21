/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

SineLabAudioProcessor::SineLabAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    for (int i = 0; i < 88; ++i) evenMorphStrength[i] = 1.0;
    harmonicCounts.resize (88);

        for (int key = 0; key < 88; ++key)
        {
            int noteNumber = key + 21;
            double fundamentalFrequency = 440.0 * std::pow (2.0, (noteNumber - 69) / 12.0);
            harmonicCounts[key] = (int) (20000 / fundamentalFrequency);
        }
    
    
    keyStartIndex.resize (88);
        keyVolume.resize (88, 1.0);
        keyVelocityScalar.resize (88, 1.0);
        keyInharmonicityB.resize (88, 0.0);
        keyDutyCycle.resize (88, 0.0);

    int runningTotal = 0;

    for (int key = 0; key < 88; ++key)
    {
        keyStartIndex[key] = runningTotal;
        runningTotal += harmonicCounts[key];
    }

    oscillators.resize (runningTotal);

    for (int key = 0; key < 88; ++key)
        {
            for (int h = 0; h < harmonicCounts[key]; ++h)
            {
                oscillators[keyStartIndex[key] + h].harmonicNumber = h + 1;
                oscillators[keyStartIndex[key] + h].ownerKey = key;

                double thisFrequency = (h + 1) * (440.0 * std::pow (2.0, (key + 21 - 69) / 12.0));
                                oscillators[keyStartIndex[key] + h].maxUpwardCents = juce::jmax (1, (int) (1200.0 * std::log2 (100000.0 / thisFrequency)));
                                oscillators[keyStartIndex[key] + h].maxDownwardCents = juce::jmin (-1, (int) (1200.0 * std::log2 (20.0 / thisFrequency)));
                                oscillators[keyStartIndex[key] + h].audibleMaxCents = juce::jmax (1, (int) (1200.0 * std::log2 (20000.0 / thisFrequency)));
            }
        }
    }



SineLabAudioProcessor::~SineLabAudioProcessor()
{
}

//==============================================================================
const juce::String SineLabAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SineLabAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SineLabAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SineLabAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SineLabAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SineLabAudioProcessor::getNumPrograms()
{
    return 1;
}

int SineLabAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SineLabAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SineLabAudioProcessor::getProgramName (int index)
{
    return {};
}

void SineLabAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SineLabAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
}

void SineLabAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SineLabAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SineLabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    double sampleTimeIncrement = 1.0 / currentSampleRate;

    for (const auto metadata : midiMessages)
        
    {
        auto message = metadata.getMessage();

        
        if (message.isNoteOn())
                {
                    int noteNumber = message.getNoteNumber();

                    if (noteNumber >= 21 && noteNumber <= 108)
                    {
                        int key = noteNumber - 21;
                        int velocity = message.getVelocity();
                        keyVelocityScalar[key] = velocity / 127.0;

                        for (int h = 0; h < harmonicCounts[key]; ++h)
                        {
                            int index = keyStartIndex[key] + h;
                            oscillators[index].rotationCos = (float) std::cos (oscillators[index].startPhase);
                            oscillators[index].rotationSin = (float) std::sin (oscillators[index].startPhase);
                            
                            oscillators[index].active = true;
                            oscillators[index].midiNoteNumber = noteNumber;
                            oscillators[index].needsFrequencyUpdate = true;
                            oscillators[index].envelopeElapsed = 0.0;
                            oscillators[index].isReleasing = false;
                        }
                    }
                }

        
        
        if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();

            if (noteNumber >= 21 && noteNumber <= 108)
            {
                int key = noteNumber - 21;

                for (int h = 0; h < harmonicCounts[key]; ++h)
                {
                    int index = keyStartIndex[key] + h;
                    oscillators[index].isReleasing = true;
                    oscillators[index].releaseElapsed = 0.0;
                    oscillators[index].levelAtReleaseStart = oscillators[index].envelopeValue;
                }
            }
        }
        
        
        
        
        
    }

    buffer.clear();

        const int numSamples = buffer.getNumSamples();
        float* const leftChannel = buffer.getNumChannels() >= 1 ? buffer.getWritePointer (0) : nullptr;
        float* const rightChannel = buffer.getNumChannels() >= 2 ? buffer.getWritePointer (1) : nullptr;
        
        const int totalOscillatorCount = (int) oscillators.size();

        for (int note = 0; note < totalOscillatorCount; ++note)
        {
            auto& osc = oscillators[note];

            if (! osc.active || osc.manuallyMuted || osc.aboveCeiling)
                continue;
            
            

            // Run parameters setup once per oscillator block instead of per-sample
            if (osc.tuningCents >= osc.audibleMaxCents || osc.tuningCents <= osc.maxDownwardCents)
                continue;
            if (osc.attackTime == 0.0)
                continue;
            if (osc.decayTime == 0.0 && osc.sustainLevel == 0.0)
                continue;
            if (osc.amplitude == 0.0)
                continue;
            

                        // Run parameters setup once per oscillator block instead of per-sample
                        if (osc.needsFrequencyUpdate)
                        {
                double baseFrequency = 440.0 * std::pow (2.0, (osc.midiNoteNumber - 69) / 12.0);
                double frequency = osc.harmonicNumber * baseFrequency * std::pow (2.0, osc.tuningCents / 1200.0);
                double angleDelta = (frequency / currentSampleRate) * 2.0 * juce::MathConstants<double>::pi;
                osc.deltaCos = (float) std::cos (angleDelta);
                osc.deltaSin = (float) std::sin (angleDelta);
                
                osc.needsFrequencyUpdate = false;
            }

            if (osc.needsPhaseUpdate)
            {
                osc.rotationCos = (float) std::cos (osc.startPhase);
                osc.rotationSin = (float) std::sin (osc.startPhase);
                
                osc.needsPhaseUpdate = false;
            }

            if (osc.needsPanUpdate)
            {
                double panAngle = (osc.pan + 1.0) * (juce::MathConstants<double>::pi / 4.0);
                osc.leftGain = (float) std::cos (panAngle);
                osc.rightGain = (float) std::sin (panAngle);
                osc.needsPanUpdate = false;
            }

            float rotationMagnitude = std::sqrt (osc.rotationCos * osc.rotationCos + osc.rotationSin * osc.rotationSin);
            if (rotationMagnitude > 1e-6f)
            {
                osc.rotationCos /= rotationMagnitude;
                osc.rotationSin /= rotationMagnitude;
            }
            else
            {
                osc.rotationCos = (float) std::cos (osc.startPhase);
                osc.rotationSin = (float) std::sin (osc.startPhase);
            }

            // Cache parameters to hardware registers for the duration of this loop
            float taperFactor = 1.0f;
            if (taperCTEnabled)
            {
                int count = harmonicCounts[osc.ownerKey];
                int h = osc.harmonicNumber - 1;
                if (count > 0)
                    taperFactor *= (float)(count - h) / (float)count;
            }
            if (taperACTEnabled && osc.activeCount > 0)
                taperFactor *= (float)(osc.activeCount - osc.activeRank) / (float)osc.activeCount;

            const float staticGains = osc.amplitude * taperFactor * (float) keyVolume[osc.ownerKey] * (float) keyVelocityScalar[osc.ownerKey];
            const float lGainCombined = osc.leftGain * staticGains;
            const float rGainCombined = osc.rightGain * staticGains;

            float rCos = osc.rotationCos;
            float rSin = osc.rotationSin;
            const float dCos = osc.deltaCos;
            const float dSin = osc.deltaSin;

            // Process all samples sequentially for this single oscillator
            for (int sample = 0; sample < numSamples; ++sample)
            {
                osc.envelopeElapsed += sampleTimeIncrement;

                if (osc.isReleasing)
                {
                    osc.releaseElapsed += sampleTimeIncrement;
                    double releaseProgress = juce::jmin (1.0, osc.releaseElapsed / osc.releaseTime);
                    osc.envelopeValue = (float) (osc.levelAtReleaseStart * (1.0 - releaseProgress));

                    if (releaseProgress >= 1.0)
                        osc.active = false;
                }
                else
                {
                    double timeIntoDecay = osc.envelopeElapsed - osc.attackTime;

                    if (timeIntoDecay < 0.0)
                    {
                        double attackProgress = osc.envelopeElapsed / osc.attackTime;
                        osc.envelopeValue = (float) juce::jmin (1.0, attackProgress);
                    }
                    else
                    {
                        double decayProgress = juce::jmin (1.0, timeIntoDecay / osc.decayTime);
                        double decayRemaining = 1.0 - decayProgress;
                                                osc.envelopeValue = (float) (osc.sustainLevel + (1.0 - osc.sustainLevel) * decayRemaining * decayRemaining * decayRemaining);


                    }
                }

                const float currentSample = rSin * osc.envelopeValue;

                // Compute complex rotation
                const float newRotationCos = rCos * dCos - rSin * dSin;
                const float newRotationSin = rSin * dCos + rCos * dSin;
                rCos = newRotationCos;
                rSin = newRotationSin;

                // Direct buffer accumulation via pointers avoids function call overhead
                if (leftChannel)
                    leftChannel[sample] += currentSample * lGainCombined;

                if (rightChannel)
                    rightChannel[sample] += currentSample * rGainCombined;
            }

            // Save current phase state back to the struct for the next block
            osc.rotationCos = rCos;
            osc.rotationSin = rSin;
        }
    
    
}


//==============================================================================
bool SineLabAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SineLabAudioProcessor::createEditor()
{
    return new SineLabAudioProcessorEditor (*this);
}

//==============================================================================
void SineLabAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("SineLabState");

    for (int i = 0; i < (int) oscillators.size(); ++i)
    {
        auto* oscXml = xml.createNewChildElement ("Oscillator");
        oscXml->setAttribute ("index", i);
        oscXml->setAttribute ("amplitude", oscillators[i].amplitude);
        oscXml->setAttribute ("manuallyMuted", oscillators[i].manuallyMuted);
        oscXml->setAttribute ("tuningCents", oscillators[i].tuningCents);
                oscXml->setAttribute ("stretchCents", oscillators[i].stretchCents);
                oscXml->setAttribute ("inharmonicityCents", oscillators[i].inharmonicityCents);
        
        oscXml->setAttribute ("startPhaseDegrees", oscillators[i].startPhaseDegrees);
        oscXml->setAttribute ("setStartPhaseDegrees", oscillators[i].setStartPhaseDegrees);
        oscXml->setAttribute ("pan", oscillators[i].pan);
        oscXml->setAttribute ("attackTime", oscillators[i].attackTime);
        oscXml->setAttribute ("decayTime", oscillators[i].decayTime);
        oscXml->setAttribute ("decayShapeRatio", oscillators[i].decayShapeRatio);
        oscXml->setAttribute ("sustainLevel", oscillators[i].sustainLevel);
        oscXml->setAttribute ("releaseTime", oscillators[i].releaseTime);
        oscXml->setAttribute ("aboveCeiling", oscillators[i].aboveCeiling);
    }
    
    for (int key = 0; key < 88; ++key)
        {
            auto* keyXml = xml.createNewChildElement ("KeyState");
            keyXml->setAttribute ("key", key);
            keyXml->setAttribute ("keyVolume", keyVolume[key]);
            keyXml->setAttribute ("inharmonicityB", keyInharmonicityB[key]);
            keyXml->setAttribute ("dutyCycle", keyDutyCycle[key]);
            keyXml->setAttribute ("evenMorphStrength", evenMorphStrength[key]);
        }
    
    
    auto* globalXml = xml.createNewChildElement ("GlobalState");
    globalXml->setAttribute ("globalToggle",        globalToggleState);
    globalXml->setAttribute ("firstHarmonicToggle", firstHarmonicToggleState);
    globalXml->setAttribute ("evensToggle",         evensToggleState);
    globalXml->setAttribute ("primesToggle",        primesToggleState);
    globalXml->setAttribute ("globalAmp", globalAmpValue);
    globalXml->setAttribute ("globalTuning", globalTuningValue);
    globalXml->setAttribute ("globalPhase", globalPhaseValue);
    globalXml->setAttribute ("globalPan", globalPanValue);
    globalXml->setAttribute ("lastAppliedPanWidth", lastAppliedPanWidth);
    globalXml->setAttribute ("globalAttack", globalAttackValue);
    globalXml->setAttribute ("globalSustain",             globalSustainValue);
    globalXml->setAttribute ("lastAppliedSustainStartKey", lastAppliedSustainStartKey);
    globalXml->setAttribute ("lastAppliedSustainA0",       lastAppliedSustainA0);
    globalXml->setAttribute ("lastAppliedSustainEndKey",   lastAppliedSustainEndKey);
    globalXml->setAttribute ("lastAppliedSustainC8",       lastAppliedSustainC8);
    globalXml->setAttribute ("lastAppliedExpKSustain",     lastAppliedExpKSustain);
    globalXml->setAttribute ("globalDecay", globalDecayValue);
    globalXml->setAttribute ("globalRelease", globalReleaseValue);
    globalXml->setAttribute ("normalizationEnabled", normalizationEnabled);
    globalXml->setAttribute ("taperCTEnabled",  taperCTEnabled);
    globalXml->setAttribute ("taperACTEnabled", taperACTEnabled);
    globalXml->setAttribute ("lastAppliedStretchA0", lastAppliedStretchA0);
    
    
    globalXml->setAttribute ("lastAppliedStretchC8", lastAppliedStretchC8);
    globalXml->setAttribute ("lastAppliedInharmonicityA0", lastAppliedInharmonicityA0);
    globalXml->setAttribute ("lastAppliedInharmonicityC8", lastAppliedInharmonicityC8);
    globalXml->setAttribute ("lastAppliedAmpA0", lastAppliedAmpA0);
    globalXml->setAttribute ("lastAppliedAmpC8", lastAppliedAmpC8);
    globalXml->setAttribute ("lastAppliedAmpStartKey", lastAppliedAmpStartKey);
    globalXml->setAttribute ("lastAppliedAmpEndKey", lastAppliedAmpEndKey);
    globalXml->setAttribute ("lastAppliedKeyVolumeA0",       lastAppliedKeyVolumeA0);
    globalXml->setAttribute ("lastAppliedKeyVolumeC8",       lastAppliedKeyVolumeC8);
    globalXml->setAttribute ("lastAppliedKeyVolumeStartKey", lastAppliedKeyVolumeStartKey);
    globalXml->setAttribute ("lastAppliedKeyVolumeEndKey",   lastAppliedKeyVolumeEndKey);
    globalXml->setAttribute ("lastAppliedExpKKeyVolume",     lastAppliedExpKKeyVolume);
    globalXml->setAttribute ("globalKeyVolumeValue",         globalKeyVolumeValue);
    globalXml->setAttribute ("lastAppliedExpKAmp", lastAppliedExpKAmp);
    globalXml->setAttribute ("lastAppliedAttackA0", lastAppliedAttackA0);
    globalXml->setAttribute ("lastAppliedAttackC8", lastAppliedAttackC8);
    globalXml->setAttribute ("lastAppliedAttackRand", lastAppliedAttackRand);
    globalXml->setAttribute ("lastAppliedReleaseRand", lastAppliedReleaseRand);
    globalXml->setAttribute ("lastAppliedDecayA0", lastAppliedDecayA0);
    globalXml->setAttribute ("lastAppliedDecayC8", lastAppliedDecayC8);
    globalXml->setAttribute ("lastAppliedDecayIIThreshold", lastAppliedDecayIIThreshold);
    globalXml->setAttribute ("lastAppliedReleaseA0", lastAppliedReleaseA0);
    globalXml->setAttribute ("lastAppliedReleaseC8", lastAppliedReleaseC8);
    globalXml->setAttribute ("lastAppliedToggleA0", lastAppliedToggleA0);
    globalXml->setAttribute ("lastAppliedToggleC8", lastAppliedToggleC8);
    globalXml->setAttribute ("lastAppliedExpKToggle",  lastAppliedExpKToggle);
    globalXml->setAttribute ("lastAppliedExpKTuning1", lastAppliedExpKTuning1);
    globalXml->setAttribute ("lastAppliedExpKTuning2", lastAppliedExpKTuning2);
    globalXml->setAttribute ("lastAppliedExpKAttack", lastAppliedExpKAttack);
    globalXml->setAttribute ("lastAppliedExpKDecay1", lastAppliedExpKDecay1);
    globalXml->setAttribute ("lastAppliedExpKDecay2", lastAppliedExpKDecay2);
    globalXml->setAttribute ("lastAppliedExpKRelease", lastAppliedExpKRelease);
    globalXml->setAttribute ("lastAppliedExpKPhase",   lastAppliedExpKPhase);
    globalXml->setAttribute ("lastAppliedPhaseA0",     lastAppliedPhaseA0);
    globalXml->setAttribute ("lastAppliedPhaseC8",     lastAppliedPhaseC8);
    globalXml->setAttribute ("lastAppliedPhaseRand",   lastAppliedPhaseRand);
    globalXml->setAttribute ("lastAppliedPhaseEvens",       lastAppliedPhaseEvens);
    globalXml->setAttribute ("lastAppliedExpKEvenMorph",    lastAppliedExpKEvenMorph);
    globalXml->setAttribute ("lastAppliedEvenMorphStartKey",lastAppliedEvenMorphStartKey);
    globalXml->setAttribute ("lastAppliedEvenMorphEndKey",  lastAppliedEvenMorphEndKey);
    globalXml->setAttribute ("lastAppliedEvenMorphA0",      lastAppliedEvenMorphA0);
    globalXml->setAttribute ("lastAppliedEvenMorphC8",      lastAppliedEvenMorphC8);

        copyXmlToBinary (xml, destData);
    
    
}

void SineLabAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    // 1. First restore global properties so oscillator processing can use them
    auto* globalXml = xml->getChildByName ("GlobalState");
    if (globalXml != nullptr)
    {
        globalToggleState        = globalXml->getBoolAttribute ("globalToggle",        true);
        firstHarmonicToggleState = globalXml->getBoolAttribute ("firstHarmonicToggle", true);
        evensToggleState         = globalXml->getBoolAttribute ("evensToggle",         true);
        primesToggleState        = globalXml->getBoolAttribute ("primesToggle",        true);
        globalAmpValue = globalXml->getDoubleAttribute ("globalAmp", 1.0);
        globalTuningValue = globalXml->getIntAttribute ("globalTuning", 0);
        globalPhaseValue = globalXml->getIntAttribute ("globalPhase", 0);
        globalPanValue = globalXml->getDoubleAttribute ("globalPan", 0.0);
        lastAppliedPanWidth = juce::jlimit (-1.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedPanWidth", 1.0));
        globalAttackValue = globalXml->getDoubleAttribute ("globalAttack", 0.0001);
        globalSustainValue          = globalXml->getDoubleAttribute ("globalSustain", 1.0);
        lastAppliedSustainStartKey  = juce::jlimit (0, 87, globalXml->getIntAttribute    ("lastAppliedSustainStartKey", 0));
        lastAppliedSustainA0        = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedSustainA0",  1.0));
        lastAppliedSustainEndKey    = juce::jlimit (0, 87, globalXml->getIntAttribute    ("lastAppliedSustainEndKey",  87));
        lastAppliedSustainC8        = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedSustainC8",  1.0));
        lastAppliedExpKSustain      = juce::jlimit (-50, 50, globalXml->getIntAttribute  ("lastAppliedExpKSustain",   0));
        globalDecayValue = globalXml->getDoubleAttribute ("globalDecay", 0.0001);
        globalReleaseValue = globalXml->getDoubleAttribute ("globalRelease", 0.0001);
        normalizationEnabled = globalXml->getBoolAttribute ("normalizationEnabled", false);
        taperCTEnabled  = globalXml->getBoolAttribute ("taperCTEnabled",  false);
        taperACTEnabled = globalXml->getBoolAttribute ("taperACTEnabled", false);
        lastAppliedStretchA0 = globalXml->getIntAttribute ("lastAppliedStretchA0", 0);
        lastAppliedStretchC8 = globalXml->getIntAttribute ("lastAppliedStretchC8", 0);
        
        lastAppliedInharmonicityA0 = juce::jlimit (-0.0015, 0.0015, globalXml->getDoubleAttribute ("lastAppliedInharmonicityA0", 0.0));
        lastAppliedInharmonicityC8 = juce::jlimit (-0.0015, 0.0015, globalXml->getDoubleAttribute ("lastAppliedInharmonicityC8", 0.0));
        lastAppliedAmpA0 = juce::jlimit (0.0, 0.5, globalXml->getDoubleAttribute ("lastAppliedAmpA0", 0.25));
        lastAppliedAmpC8 = juce::jlimit (0.0, 0.5, globalXml->getDoubleAttribute ("lastAppliedAmpC8", 0.25));
        lastAppliedAmpStartKey = juce::jlimit (0, 87, globalXml->getIntAttribute ("lastAppliedAmpStartKey", 0));
        lastAppliedAmpEndKey   = juce::jlimit (0, 87, globalXml->getIntAttribute ("lastAppliedAmpEndKey", 87));
        lastAppliedKeyVolumeA0       = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedKeyVolumeA0", 1.0));
        lastAppliedKeyVolumeC8       = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedKeyVolumeC8", 1.0));
        lastAppliedKeyVolumeStartKey = juce::jlimit (0, 87, globalXml->getIntAttribute ("lastAppliedKeyVolumeStartKey", 0));
        lastAppliedKeyVolumeEndKey   = juce::jlimit (0, 87, globalXml->getIntAttribute ("lastAppliedKeyVolumeEndKey", 87));
        lastAppliedExpKKeyVolume     = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKKeyVolume", 4));
        globalKeyVolumeValue         = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("globalKeyVolumeValue", 1.0));
        lastAppliedExpKAmp = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKAmp", 4));
        lastAppliedAttackA0 = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedAttackA0", 0.0001));
        lastAppliedAttackC8 = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedAttackC8", 0.0001));
        lastAppliedAttackRand = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedAttackRand", 0.0));
        lastAppliedReleaseRand = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedReleaseRand", 0.0));
        lastAppliedDecayA0 = juce::jlimit (0.0, 10.0, globalXml->getDoubleAttribute ("lastAppliedDecayA0", 0.0));
        lastAppliedDecayC8 = juce::jlimit (0.0, 10.0, globalXml->getDoubleAttribute ("lastAppliedDecayC8", 0.0));
        lastAppliedDecayIIThreshold = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedDecayIIThreshold", 0.0));
        lastAppliedReleaseA0 = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedReleaseA0", 0.0001));
        lastAppliedReleaseC8 = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedReleaseC8", 0.0001));
        lastAppliedToggleA0 = juce::jlimit (1, 727, globalXml->getIntAttribute ("lastAppliedToggleA0", 1));
        lastAppliedToggleC8 = juce::jlimit (1,   4, globalXml->getIntAttribute ("lastAppliedToggleC8", 1));
        lastAppliedExpKToggle  = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKToggle",  4));
        lastAppliedExpKTuning1 = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKTuning1", 4));
        lastAppliedExpKTuning2 = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKTuning2", 4));
        lastAppliedExpKAttack   = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKAttack",   4));
        lastAppliedExpKDecay1   = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKDecay1",   4));
        lastAppliedExpKDecay2   = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKDecay2",   4));
        lastAppliedExpKRelease  = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKRelease",  4));
        lastAppliedExpKPhase    = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKPhase",    4));
        lastAppliedPhaseA0   = juce::jlimit (1, 360, globalXml->getIntAttribute ("lastAppliedPhaseA0", 1));
        lastAppliedPhaseC8   = juce::jlimit (1, 360, globalXml->getIntAttribute ("lastAppliedPhaseC8", 1));
        lastAppliedPhaseRand  = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedPhaseRand", 0.0));
        lastAppliedPhaseEvens        = juce::jlimit (1, 360, globalXml->getIntAttribute ("lastAppliedPhaseEvens", 1));
        lastAppliedExpKEvenMorph     = juce::jlimit (-20, 20, globalXml->getIntAttribute ("lastAppliedExpKEvenMorph", 4));
        lastAppliedEvenMorphStartKey = juce::jlimit (0, 87,   globalXml->getIntAttribute    ("lastAppliedEvenMorphStartKey", 0));
        lastAppliedEvenMorphEndKey   = juce::jlimit (0, 87,   globalXml->getIntAttribute    ("lastAppliedEvenMorphEndKey",   87));
        lastAppliedEvenMorphA0       = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedEvenMorphA0",       1.0));
        lastAppliedEvenMorphC8       = juce::jlimit (0.0, 1.0, globalXml->getDoubleAttribute ("lastAppliedEvenMorphC8",       0.0));
        
            }
    

    // 2. Restore individual key volumes and tuning graph states
    for (auto* childXml : xml->getChildIterator())
        {
            if (childXml->getTagName() == "KeyState")
            {
                int key = childXml->getIntAttribute ("key");
                if (key >= 0 && key < 88)
                {
                    keyVolume[key] = childXml->getDoubleAttribute ("keyVolume", 1.0);
                    keyInharmonicityB[key] = juce::jlimit (-0.0015, 0.0015, childXml->getDoubleAttribute ("inharmonicityB", 0.0));
                    keyDutyCycle[key]       = juce::jlimit (0.0, 0.5, childXml->getDoubleAttribute ("dutyCycle", 0.0));
                    evenMorphStrength[key]  = juce::jlimit (0.0, 1.0, childXml->getDoubleAttribute ("evenMorphStrength", 1.0));
                    
                }
            }
        }

    // 3. Restore oscillator array parameters
    for (auto* oscXml : xml->getChildIterator())
    {
        if (oscXml->getTagName() != "Oscillator")
            continue;

        int index = oscXml->getIntAttribute ("index");

        if (index >= 0 && index < (int) oscillators.size())
        {
            oscillators[index].amplitude = (double) oscXml->getDoubleAttribute ("amplitude", 1.0);
            oscillators[index].manuallyMuted = oscXml->getBoolAttribute ("manuallyMuted", false);
            
            
            oscillators[index].stretchCents = oscXml->getIntAttribute ("stretchCents", 0);
                        oscillators[index].inharmonicityCents = oscXml->getIntAttribute ("inharmonicityCents", 0);
                        oscillators[index].recombineTuningCents();
            
            oscillators[index].startPhaseDegrees = oscXml->getIntAttribute ("startPhaseDegrees", 0);
            oscillators[index].setStartPhaseDegrees = oscXml->getIntAttribute ("setStartPhaseDegrees", oscillators[index].startPhaseDegrees);
            oscillators[index].startPhase = oscillators[index].startPhaseDegrees * (juce::MathConstants<double>::pi / 180.0);
            oscillators[index].pan = oscXml->getDoubleAttribute ("pan", 0.0);
            oscillators[index].attackTime    = oscXml->getDoubleAttribute ("attackTime", 0.0001);
            oscillators[index].setAttackTime = oscillators[index].attackTime;
            oscillators[index].decayTime = oscXml->getDoubleAttribute ("decayTime", 0.0001);
            oscillators[index].decayShapeRatio = juce::jlimit (0.0, 1.0, oscXml->getDoubleAttribute ("decayShapeRatio", 1.0));
            oscillators[index].sustainLevel = oscXml->getDoubleAttribute ("sustainLevel", 1.0);
            oscillators[index].releaseTime    = oscXml->getDoubleAttribute ("releaseTime", 0.0001);
            oscillators[index].setReleaseTime = oscillators[index].releaseTime;
            oscillators[index].aboveCeiling = oscXml->getBoolAttribute ("aboveCeiling", false);
            
            // Force hardware registers to recalculate using restored settings
            oscillators[index].needsFrequencyUpdate = true;
            oscillators[index].needsPhaseUpdate = true;
            oscillators[index].needsPanUpdate = true;
        }
    }

    updateActiveRanks();
    sendChangeMessage();
}

void SineLabAudioProcessor::updateActiveRanks()
{
    for (int key = 0; key < 88; ++key)
    {
        int startIndex = keyStartIndex[key];
        int count      = harmonicCounts[key];

        int activeCount = 0;
        for (int h = 0; h < count; ++h)
        {
            auto& osc = oscillators[startIndex + h];
            if (! osc.manuallyMuted && ! osc.aboveCeiling)
                ++activeCount;
        }

        int activeRank = 0;
        for (int h = 0; h < count; ++h)
        {
            auto& osc = oscillators[startIndex + h];
            osc.activeCount = activeCount;
            if (! osc.manuallyMuted && ! osc.aboveCeiling)
                osc.activeRank = activeRank++;
            else
                osc.activeRank = 0;
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SineLabAudioProcessor();
}
