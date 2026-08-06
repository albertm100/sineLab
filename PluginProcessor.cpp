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
    // Maximum possible release time — lets DAW bounces wait for the final
    // release tail instead of cutting at the last note-off. Read-only answer
    // to the host; affects no parameter and no live behavior.
    return 1.0;
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
                            oscillators[index].envelopeElapsed = 0.0f;
                            oscillators[index].isReleasing = false;
                            oscillators[index].inAttack = true;
                        }
                        keySounding[key] = true;
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
        float* const leftChannel  = buffer.getNumChannels() >= 1 ? buffer.getWritePointer (0) : nullptr;
        float* const rightChannel = buffer.getNumChannels() >= 2 ? buffer.getWritePointer (1) : nullptr;
        const bool hasLeft  = (leftChannel  != nullptr);
        const bool hasRight = (rightChannel != nullptr);
        
        for (int key = 0; key < 88; ++key)
        {
            if (! keySounding[key]) continue;

            int startIdx = keyStartIndex[key];
            int count    = harmonicCounts[key];
            bool anyStillActive = false;

            for (int h = 0; h < count; ++h)
            {
            auto& osc = oscillators[startIdx + h];

            if (! osc.active)
                continue;

            // The key counts as sounding while any oscillator is still active,
            // even if currently silenced — so restoring a parameter mid-note
            // is immediately audible without re-striking the key
            anyStillActive = true;

            if (! osc.isAudible())
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

            // Precompute reciprocals so the sample loop uses multiplies, not divisions (all float)
            const float invAttackTime  = osc.attackTime  > 0.0f ? 1.0f / osc.attackTime  : 0.0f;
            const float invDecayTime   = osc.decayTime   > 0.0f ? 1.0f / osc.decayTime   : 0.0f;
            const float invReleaseTime = osc.releaseTime > 0.0f ? 1.0f / osc.releaseTime : 0.0f;
            const float sampleInc      = (float) sampleTimeIncrement;

            // An envelope time of 0 means instantaneous, never "stuck at the start"
            const bool instantDecay   = (osc.decayTime   <= 0.0f);
            const bool instantRelease = (osc.releaseTime <= 0.0f);
            if (osc.attackTime <= 0.0f)
                osc.inAttack = false;

            // Phase-separated sample loops: envelope segment is determined once before each
            // loop, so no per-sample branching on isReleasing / inAttack in the hot path.
            // Transitions are handled by breaking from one loop and falling into the next.
            int s = 0;

            // RELEASE phase
            if (osc.isReleasing)
            {
                float startVal = osc.envelopeValue;
                float endVal = 0.0f;
                int limit = numSamples;
                
                if (osc.releaseTime > 0.0f)
                {
                    double remainingTime = osc.releaseTime - osc.releaseElapsed;
                    int samplesToFinish = (int)std::ceil(remainingTime / sampleInc);
                    if (samplesToFinish < numSamples)
                    {
                        limit = std::max(0, samplesToFinish);
                    }
                    else
                    {
                        double nextReleaseElapsed = osc.releaseElapsed + numSamples * sampleInc;
                        float endProgress = (float)(nextReleaseElapsed * invReleaseTime);
                        endVal = osc.levelAtReleaseStart * (1.0f - std::min(1.0f, endProgress));
                    }
                }
                
                float env = startVal;
                float envStep = (limit > 0) ? (endVal - startVal) / limit : 0.0f;
                
                for (; s < limit; ++s)
                {
                    const float sv = rSin * env;
                    const float nc = rCos * dCos - rSin * dSin;
                    const float ns = rSin * dCos + rCos * dSin;
                    rCos = nc; rSin = ns;
                    if (hasLeft)  leftChannel[s]  += sv * lGainCombined;
                    if (hasRight) rightChannel[s] += sv * rGainCombined;
                    env += envStep;
                }
                
                osc.releaseElapsed += limit * sampleInc;
                osc.envelopeElapsed += limit * sampleInc;
                osc.envelopeValue = env;
                
                if (limit < numSamples)
                {
                    osc.active = false;
                }
            }

            // ATTACK phase
            if (osc.active && osc.inAttack)
            {
                float startVal = osc.envelopeValue;
                float endVal = 1.0f;
                int limit = numSamples - s;
                
                if (osc.attackTime > 0.0f)
                {
                    double remainingTime = osc.attackTime - osc.envelopeElapsed;
                    int samplesToFinish = (int)std::ceil(remainingTime / sampleInc);
                    if (samplesToFinish < (numSamples - s))
                    {
                        limit = std::max(0, samplesToFinish);
                    }
                    else
                    {
                        double nextEnvelopeElapsed = osc.envelopeElapsed + limit * sampleInc;
                        endVal = (float)(nextEnvelopeElapsed * invAttackTime);
                    }
                }
                
                float env = startVal;
                float envStep = (limit > 0) ? (endVal - startVal) / limit : 0.0f;
                int endS = s + limit;
                
                for (; s < endS; ++s)
                {
                    const float sv = rSin * env;
                    const float nc = rCos * dCos - rSin * dSin;
                    const float ns = rSin * dCos + rCos * dSin;
                    rCos = nc; rSin = ns;
                    if (hasLeft)  leftChannel[s]  += sv * lGainCombined;
                    if (hasRight) rightChannel[s] += sv * rGainCombined;
                    env += envStep;
                }
                
                osc.envelopeElapsed += limit * sampleInc;
                osc.envelopeValue = env;
                
                if (osc.envelopeElapsed >= osc.attackTime)
                {
                    osc.envelopeValue = 1.0f;
                    osc.inAttack = false;
                }
            }

            // DECAY / SUSTAIN phase
            if (osc.active)
            {
                float startVal = osc.envelopeValue;
                int limit = numSamples - s;
                float endVal = osc.sustainLevel;
                
                if (osc.decayTime > 0.0f)
                {
                    double timeIntoDecay = osc.envelopeElapsed - osc.attackTime;
                    double remainingTime = osc.decayTime - timeIntoDecay;
                    int samplesToFinish = (int)std::ceil(remainingTime / sampleInc);
                    
                    if (samplesToFinish < limit)
                    {
                        limit = std::max(0, samplesToFinish);
                    }
                    else
                    {
                        double nextEnvelopeElapsed = osc.envelopeElapsed + limit * sampleInc;
                        double nextTimeIntoDecay = nextEnvelopeElapsed - osc.attackTime;
                        float decayProgress = (float)(nextTimeIntoDecay * invDecayTime);
                        float decayRemaining = 1.0f - std::min(1.0f, decayProgress);
                        endVal = osc.sustainLevel + (1.0f - osc.sustainLevel) * decayRemaining * decayRemaining * decayRemaining;
                    }
                }
                
                float env = startVal;
                float envStep = (limit > 0) ? (endVal - startVal) / limit : 0.0f;
                int endS = s + limit;
                
                for (; s < endS; ++s)
                {
                    const float sv = rSin * env;
                    const float nc = rCos * dCos - rSin * dSin;
                    const float ns = rSin * dCos + rCos * dSin;
                    rCos = nc; rSin = ns;
                    if (hasLeft)  leftChannel[s]  += sv * lGainCombined;
                    if (hasRight) rightChannel[s] += sv * rGainCombined;
                    env += envStep;
                }
                osc.envelopeElapsed += limit * sampleInc;
                osc.envelopeValue = env;
                
                if (s < numSamples)
                {
                    int remainingSamples = numSamples - s;
                    for (; s < numSamples; ++s)
                    {
                        const float sv = rSin * osc.sustainLevel;
                        const float nc = rCos * dCos - rSin * dSin;
                        const float ns = rSin * dCos + rCos * dSin;
                        rCos = nc; rSin = ns;
                        if (hasLeft)  leftChannel[s]  += sv * lGainCombined;
                        if (hasRight) rightChannel[s] += sv * rGainCombined;
                    }
                    osc.envelopeElapsed += remainingSamples * sampleInc;
                    osc.envelopeValue = osc.sustainLevel;
                }
            }

            // Save current phase state back to the struct for the next block
            osc.rotationCos = rCos;
            osc.rotationSin = rSin;
            } // end harmonic loop

            if (! anyStillActive)
                keySounding[key] = false;
        } // end key loop
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
    juce::XmlElement root ("SineLab");

    root.setAttribute ("globalToggleState",        (int) globalToggleState);
    root.setAttribute ("firstHarmonicToggleState", (int) firstHarmonicToggleState);
    root.setAttribute ("evensToggleState",         (int) evensToggleState);
    root.setAttribute ("primesToggleState",        (int) primesToggleState);
    root.setAttribute ("everyNToggleState",        (int) everyNToggleState);
    root.setAttribute ("everyNValue",              everyNValue);
    root.setAttribute ("globalAmpValue",           globalAmpValue);
    root.setAttribute ("globalTuningValue",        globalTuningValue);
    root.setAttribute ("globalPhaseValue",         globalPhaseValue);
    root.setAttribute ("globalPanValue",           globalPanValue);
    root.setAttribute ("lastAppliedPanWidth",      lastAppliedPanWidth);
    root.setAttribute ("globalAttackValue",        globalAttackValue);
    root.setAttribute ("globalSustainValue",       globalSustainValue);
    root.setAttribute ("lastAppliedSustainStartKey", lastAppliedSustainStartKey);
    root.setAttribute ("lastAppliedSustainEndKey",   lastAppliedSustainEndKey);
    root.setAttribute ("lastAppliedExpKSustain",     lastAppliedExpKSustain);
    root.setAttribute ("globalDecayValue",         globalDecayValue);
    root.setAttribute ("globalReleaseValue",       globalReleaseValue);
    root.setAttribute ("normalizationEnabled",     (int) normalizationEnabled);
    root.setAttribute ("taperCTEnabled",           (int) taperCTEnabled);
    root.setAttribute ("taperACTEnabled",          (int) taperACTEnabled);
    root.setAttribute ("lastAppliedAmpStartKey",       lastAppliedAmpStartKey);
    root.setAttribute ("lastAppliedAmpEndKey",         lastAppliedAmpEndKey);
    root.setAttribute ("lastAppliedKeyVolumeStartKey", lastAppliedKeyVolumeStartKey);
    root.setAttribute ("lastAppliedKeyVolumeEndKey",   lastAppliedKeyVolumeEndKey);
    root.setAttribute ("lastAppliedExpKKeyVolume",     lastAppliedExpKKeyVolume);
    root.setAttribute ("lastAppliedExpKAmp",           lastAppliedExpKAmp);
    root.setAttribute ("lastAppliedDecayIIThreshold",  lastAppliedDecayIIThreshold);
    root.setAttribute ("lastAppliedToggleA0",          lastAppliedToggleA0);
    root.setAttribute ("lastAppliedToggleC8",          lastAppliedToggleC8);
    root.setAttribute ("lastAppliedExpKToggle",        lastAppliedExpKToggle);
    root.setAttribute ("lastAppliedExpKTuning1",       lastAppliedExpKTuning1);
    root.setAttribute ("lastAppliedExpKTuning2",       lastAppliedExpKTuning2);
    root.setAttribute ("lastAppliedExpKAttack",        lastAppliedExpKAttack);
    root.setAttribute ("lastAppliedExpKDecay1",        lastAppliedExpKDecay1);
    root.setAttribute ("lastAppliedExpKDecay2",        lastAppliedExpKDecay2);
    root.setAttribute ("lastAppliedExpKRelease",       lastAppliedExpKRelease);
    root.setAttribute ("lastAppliedExpKPhase",         lastAppliedExpKPhase);
    root.setAttribute ("lastAppliedExpKEvenMorph",     lastAppliedExpKEvenMorph);
    root.setAttribute ("lastAppliedEvenMorphStartKey", lastAppliedEvenMorphStartKey);
    root.setAttribute ("lastAppliedEvenMorphEndKey",   lastAppliedEvenMorphEndKey);
    root.setAttribute ("lastAppliedNToNSquaredStartKey", lastAppliedNToNSquaredStartKey);
    root.setAttribute ("lastAppliedNToNSquaredEndKey",   lastAppliedNToNSquaredEndKey);
    root.setAttribute ("lastAppliedExpKNToNSquared",     lastAppliedExpKNToNSquared);
    root.setAttribute ("lastAppliedAttackRand",         lastAppliedAttackRand);
    root.setAttribute ("lastAppliedPhaseRand",          lastAppliedPhaseRand);
    root.setAttribute ("lastAppliedReleaseRand",        lastAppliedReleaseRand);
    root.setAttribute ("lastAppliedAmpA0",           lastAppliedAmpA0);
    root.setAttribute ("lastAppliedAmpC8",           lastAppliedAmpC8);
    root.setAttribute ("lastAppliedKeyVolumeA0",     lastAppliedKeyVolumeA0);
    root.setAttribute ("lastAppliedKeyVolumeC8",     lastAppliedKeyVolumeC8);
    root.setAttribute ("lastAppliedEvenMorphA0",     lastAppliedEvenMorphA0);
    root.setAttribute ("lastAppliedEvenMorphC8",     lastAppliedEvenMorphC8);
    root.setAttribute ("lastAppliedStretchA0",       lastAppliedStretchA0);
    root.setAttribute ("lastAppliedStretchC8",       lastAppliedStretchC8);
    root.setAttribute ("lastAppliedInharmonicityA0", lastAppliedInharmonicityA0);
    root.setAttribute ("lastAppliedInharmonicityC8", lastAppliedInharmonicityC8);
    root.setAttribute ("lastAppliedPhaseA0",         lastAppliedPhaseA0);
    root.setAttribute ("lastAppliedPhaseC8",         lastAppliedPhaseC8);
    root.setAttribute ("lastAppliedAttackA0",        lastAppliedAttackA0);
    root.setAttribute ("lastAppliedAttackC8",        lastAppliedAttackC8);
    root.setAttribute ("lastAppliedDecayA0",         lastAppliedDecayA0);
    root.setAttribute ("lastAppliedDecayC8",         lastAppliedDecayC8);
    root.setAttribute ("lastAppliedSustainA0",       lastAppliedSustainA0);
    root.setAttribute ("lastAppliedSustainC8",       lastAppliedSustainC8);
    root.setAttribute ("lastAppliedReleaseA0",       lastAppliedReleaseA0);
    root.setAttribute ("lastAppliedReleaseC8",       lastAppliedReleaseC8);
    root.setAttribute ("lastAppliedIntegralValue",   lastAppliedIntegralValue);

    for (int key = 0; key < 88; ++key)
    {
        auto* k = root.createNewChildElement ("Key");
        k->setAttribute ("kv",  keyVolume[key]);
        k->setAttribute ("kb",  keyInharmonicityB[key]);
        k->setAttribute ("kdc", keyDutyCycle[key]);
        k->setAttribute ("kem", evenMorphStrength[key]);
    }

    for (int i = 0; i < (int) oscillators.size(); ++i)
    {
        auto* o = root.createNewChildElement ("O");
        o->setAttribute ("a",   oscillators[i].amplitude);
        o->setAttribute ("m",   (int) oscillators[i].manuallyMuted);
        o->setAttribute ("tc",  oscillators[i].tuningCents);
        o->setAttribute ("sc",  oscillators[i].stretchCents);
        o->setAttribute ("ic",  oscillators[i].inharmonicityCents);
        o->setAttribute ("spd", oscillators[i].startPhaseDegrees);
        o->setAttribute ("sspd",oscillators[i].setStartPhaseDegrees);
        o->setAttribute ("pan", oscillators[i].pan);
        o->setAttribute ("at",  oscillators[i].attackTime);
        o->setAttribute ("dt",  oscillators[i].decayTime);
        o->setAttribute ("dsr", oscillators[i].decayShapeRatio);
        o->setAttribute ("sl",  oscillators[i].sustainLevel);
        o->setAttribute ("rt",  oscillators[i].releaseTime);
        o->setAttribute ("ac",  (int) oscillators[i].aboveCeiling);
    }

    copyXmlToBinary (root, destData);
}

void SineLabAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || xml->getTagName() != "SineLab")
        return;

    globalToggleState        = xml->getBoolAttribute ("globalToggleState",        true);
    firstHarmonicToggleState = xml->getBoolAttribute ("firstHarmonicToggleState", true);
    evensToggleState         = xml->getBoolAttribute ("evensToggleState",         true);
    primesToggleState        = xml->getBoolAttribute ("primesToggleState",        true);
    everyNToggleState        = xml->getBoolAttribute ("everyNToggleState",        true);
    everyNValue              = juce::jlimit (2, 999, xml->getIntAttribute ("everyNValue", 3));
    globalAmpValue           = xml->getDoubleAttribute ("globalAmpValue",         1.0);
    globalTuningValue        = xml->getIntAttribute    ("globalTuningValue",      0);
    globalPhaseValue         = xml->getIntAttribute    ("globalPhaseValue",       0);
    globalPanValue           = xml->getDoubleAttribute ("globalPanValue",         0.0);
    lastAppliedPanWidth      = juce::jlimit (-1.0, 1.0, xml->getDoubleAttribute ("lastAppliedPanWidth", 1.0));
    globalAttackValue        = xml->getDoubleAttribute ("globalAttackValue",      0.0);
    globalSustainValue       = xml->getDoubleAttribute ("globalSustainValue",     1.0);
    lastAppliedSustainStartKey = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedSustainStartKey", 0));
    lastAppliedSustainEndKey   = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedSustainEndKey",   87));
    lastAppliedExpKSustain     = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKSustain",   0));
    globalDecayValue         = xml->getDoubleAttribute ("globalDecayValue",       0.0001);
    globalReleaseValue       = xml->getDoubleAttribute ("globalReleaseValue",     0.0001);
    normalizationEnabled     = xml->getBoolAttribute   ("normalizationEnabled",   false);
    taperCTEnabled           = xml->getBoolAttribute   ("taperCTEnabled",         false);
    taperACTEnabled          = xml->getBoolAttribute   ("taperACTEnabled",        false);
    lastAppliedAmpStartKey       = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedAmpStartKey",       0));
    lastAppliedAmpEndKey         = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedAmpEndKey",         87));
    lastAppliedKeyVolumeStartKey = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedKeyVolumeStartKey", 0));
    lastAppliedKeyVolumeEndKey   = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedKeyVolumeEndKey",   87));
    lastAppliedExpKKeyVolume     = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKKeyVolume",   4));
    lastAppliedExpKAmp           = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKAmp",         4));
    lastAppliedDecayIIThreshold  = juce::jlimit (0.0, 1.0, xml->getDoubleAttribute ("lastAppliedDecayIIThreshold", 0.0));
    lastAppliedToggleA0          = juce::jlimit (1, 727, xml->getIntAttribute ("lastAppliedToggleA0", 1));
    lastAppliedToggleC8          = juce::jlimit (1, 4,   xml->getIntAttribute ("lastAppliedToggleC8", 1));
    lastAppliedExpKToggle        = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKToggle",      4));
    lastAppliedExpKTuning1       = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKTuning1",     4));
    lastAppliedExpKTuning2       = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKTuning2",     4));
    lastAppliedExpKAttack        = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKAttack",      4));
    lastAppliedExpKDecay1        = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKDecay1",      4));
    lastAppliedExpKDecay2        = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKDecay2",      4));
    lastAppliedExpKRelease       = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKRelease",     4));
    lastAppliedExpKPhase         = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKPhase",       4));
    lastAppliedExpKEvenMorph     = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKEvenMorph",   4));
    lastAppliedEvenMorphStartKey = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedEvenMorphStartKey", 0));
    lastAppliedEvenMorphEndKey   = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedEvenMorphEndKey",   87));
    lastAppliedNToNSquaredStartKey = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedNToNSquaredStartKey", 0));
    lastAppliedNToNSquaredEndKey   = juce::jlimit (0, 87, xml->getIntAttribute ("lastAppliedNToNSquaredEndKey",   87));
    lastAppliedExpKNToNSquared     = juce::jlimit (-50, 50, xml->getIntAttribute ("lastAppliedExpKNToNSquared",   4));
    lastAppliedAttackRand        = juce::jlimit (0.0, 1.0, xml->getDoubleAttribute ("lastAppliedAttackRand",       0.0));
    lastAppliedPhaseRand         = juce::jlimit (0.0, 1.0, xml->getDoubleAttribute ("lastAppliedPhaseRand",        0.0));
    lastAppliedReleaseRand       = juce::jlimit (0.0, 1.0, xml->getDoubleAttribute ("lastAppliedReleaseRand",      0.0));
    lastAppliedAmpA0           = xml->getDoubleAttribute ("lastAppliedAmpA0",           0.0);
    lastAppliedAmpC8           = xml->getDoubleAttribute ("lastAppliedAmpC8",           0.0);
    lastAppliedKeyVolumeA0     = xml->getDoubleAttribute ("lastAppliedKeyVolumeA0",     1.0);
    lastAppliedKeyVolumeC8     = xml->getDoubleAttribute ("lastAppliedKeyVolumeC8",     1.0);
    lastAppliedEvenMorphA0     = xml->getDoubleAttribute ("lastAppliedEvenMorphA0",     0.0);
    lastAppliedEvenMorphC8     = xml->getDoubleAttribute ("lastAppliedEvenMorphC8",     0.0);
    lastAppliedStretchA0       = xml->getIntAttribute    ("lastAppliedStretchA0",       0);
    lastAppliedStretchC8       = xml->getIntAttribute    ("lastAppliedStretchC8",       0);
    lastAppliedInharmonicityA0 = xml->getDoubleAttribute ("lastAppliedInharmonicityA0", 0.0);
    lastAppliedInharmonicityC8 = xml->getDoubleAttribute ("lastAppliedInharmonicityC8", 0.0);
    lastAppliedPhaseA0         = xml->getIntAttribute    ("lastAppliedPhaseA0",         180);
    lastAppliedPhaseC8         = xml->getIntAttribute    ("lastAppliedPhaseC8",         180);
    lastAppliedAttackA0        = xml->getDoubleAttribute ("lastAppliedAttackA0",        0.0);
    lastAppliedAttackC8        = xml->getDoubleAttribute ("lastAppliedAttackC8",        0.0);
    lastAppliedDecayA0         = xml->getDoubleAttribute ("lastAppliedDecayA0",         0.5);
    lastAppliedDecayC8         = xml->getDoubleAttribute ("lastAppliedDecayC8",         0.5);
    lastAppliedSustainA0       = xml->getDoubleAttribute ("lastAppliedSustainA0",       1.0);
    lastAppliedSustainC8       = xml->getDoubleAttribute ("lastAppliedSustainC8",       1.0);
    lastAppliedReleaseA0       = xml->getDoubleAttribute ("lastAppliedReleaseA0",       0.0);
    lastAppliedReleaseC8       = xml->getDoubleAttribute ("lastAppliedReleaseC8",       0.0);
    lastAppliedIntegralValue   = juce::jlimit (0.0, 1.0, xml->getDoubleAttribute ("lastAppliedIntegralValue", 0.0));

    int keyIdx = 0;
    for (auto* child : xml->getChildIterator())
    {
        if (child->getTagName() == "Key" && keyIdx < 88)
        {
            keyVolume[keyIdx]         = child->getDoubleAttribute ("kv",  1.0);
            keyInharmonicityB[keyIdx] = child->getDoubleAttribute ("kb",  0.0);
            keyDutyCycle[keyIdx]      = child->getDoubleAttribute ("kdc", 0.5);
            evenMorphStrength[keyIdx] = child->getDoubleAttribute ("kem", 0.0);
            ++keyIdx;
        }
    }

    int oscIdx = 0;
    for (auto* child : xml->getChildIterator())
    {
        if (child->getTagName() == "O" && oscIdx < (int) oscillators.size())
        {
            oscillators[oscIdx].amplitude          = child->getDoubleAttribute ("a",   1.0);
            oscillators[oscIdx].manuallyMuted       = child->getBoolAttribute   ("m",   false);
            oscillators[oscIdx].tuningCents         = child->getIntAttribute    ("tc",  0);
            oscillators[oscIdx].stretchCents        = child->getIntAttribute    ("sc",  0);
            oscillators[oscIdx].inharmonicityCents  = child->getIntAttribute    ("ic",  0);
            oscillators[oscIdx].recombineTuningCents();
            oscillators[oscIdx].startPhaseDegrees    = child->getIntAttribute   ("spd", 0);
            oscillators[oscIdx].setStartPhaseDegrees = child->getIntAttribute   ("sspd",0);
            oscillators[oscIdx].startPhase = oscillators[oscIdx].startPhaseDegrees * (juce::MathConstants<double>::pi / 180.0);
            oscillators[oscIdx].pan              = child->getDoubleAttribute ("pan", 0.0);
            oscillators[oscIdx].attackTime       = child->getDoubleAttribute ("at",  0.0);
            oscillators[oscIdx].setAttackTime    = oscillators[oscIdx].attackTime;
            oscillators[oscIdx].decayTime        = child->getDoubleAttribute ("dt",  0.5);
            oscillators[oscIdx].decayShapeRatio  = child->getDoubleAttribute ("dsr", 1.0);
            oscillators[oscIdx].sustainLevel     = child->getDoubleAttribute ("sl",  1.0);
            oscillators[oscIdx].releaseTime      = child->getDoubleAttribute ("rt",  0.0);
            oscillators[oscIdx].setReleaseTime   = oscillators[oscIdx].releaseTime;
            oscillators[oscIdx].aboveCeiling     = child->getBoolAttribute   ("ac",  false);
            oscillators[oscIdx].needsFrequencyUpdate = true;
            oscillators[oscIdx].needsPhaseUpdate     = true;
            oscillators[oscIdx].needsPanUpdate       = true;
            ++oscIdx;
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
            if (oscillators[startIndex + h].isAudible())
                ++activeCount;

        int activeRank = 0;
        for (int h = 0; h < count; ++h)
        {
            auto& osc = oscillators[startIndex + h];
            osc.activeCount = activeCount;
            if (osc.isAudible())
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
