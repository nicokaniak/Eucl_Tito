/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr double kStepLengthPPQ = 0.25;
    constexpr double kGateLengthPPQ = 0.10;
}

//==============================================================================
Eucl_TitoAudioProcessor::Eucl_TitoAudioProcessor()
#if ! JucePlugin_IsMidiEffect && ! defined (JucePlugin_PreferredChannelConfigurations)
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
}

Eucl_TitoAudioProcessor::~Eucl_TitoAudioProcessor()
{
}

//==============================================================================
const juce::String Eucl_TitoAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Eucl_TitoAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Eucl_TitoAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Eucl_TitoAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Eucl_TitoAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Eucl_TitoAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Eucl_TitoAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Eucl_TitoAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Eucl_TitoAudioProcessor::getProgramName (int index)
{
    return {};
}

void Eucl_TitoAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Eucl_TitoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void Eucl_TitoAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Eucl_TitoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void Eucl_TitoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    // This instrument only generates MIDI; force the audio buffer to silence.
    buffer.clear();

    midiMessages.clear();

    const double sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return;

    // Step 4: Derive timing from host playhead if available, otherwise advance our own clock.
    juce::AudioPlayHead::CurrentPositionInfo posInfo;
    auto* playHeadPtr = getPlayHead();
    const bool hasPosition = (playHeadPtr != nullptr && playHeadPtr->getCurrentPosition (posInfo));

    const double bpm = (hasPosition && posInfo.bpm > 0.0) ? posInfo.bpm : 120.0;
    const double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double ppqPerSample = samplesPerBeat > 0.0 ? 1.0 / samplesPerBeat : 0.0;
    if (ppqPerSample <= 0.0)
        return;

    double blockStartPPQ = localClockPpq;
    if (hasPosition && posInfo.isPlaying)
    {
        if (! hostPositionInitialised || std::abs (posInfo.ppqPosition - lastHostPpq) > 0.001)
        {
            blockStartPPQ = posInfo.ppqPosition;
            localClockPpq = blockStartPPQ;
            hostPositionInitialised = true;
        }

        lastHostPpq = posInfo.ppqPosition;
    }

    const double blockEndPPQ = blockStartPPQ + (buffer.getNumSamples() * ppqPerSample);
    localClockPpq = blockEndPPQ;

    const double sequenceLengthPPQ = kNumSteps * kStepLengthPPQ;
    if (sequenceLengthPPQ <= 0.0)
        return;

    const int firstLoop = (int) std::floor (blockStartPPQ / sequenceLengthPPQ);
    const int lastLoop  = (int) std::floor ((blockEndPPQ - 1.0e-9) / sequenceLengthPPQ);

    // Step 5: Emit MIDI events in-range for this audio block (looping pattern).
    for (int loopIndex = firstLoop; loopIndex <= lastLoop; ++loopIndex)
    {
        const double loopOffset = loopIndex * sequenceLengthPPQ;
        for (int step = 0; step < kNumSteps; ++step)
        {
            if (binaryPattern[(size_t) step] == 0)
                continue;

            const double noteOnPPQ  = loopOffset + (step * kStepLengthPPQ);
            const double noteOffPPQ = noteOnPPQ + kGateLengthPPQ;

            if (noteOnPPQ >= blockStartPPQ && noteOnPPQ < blockEndPPQ)
            {
                const int sampleOffset = (int) std::round ((noteOnPPQ - blockStartPPQ) / ppqPerSample);
                midiMessages.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100),
                                       juce::jlimit (0, buffer.getNumSamples() - 1, sampleOffset));
            }

            if (noteOffPPQ >= blockStartPPQ && noteOffPPQ < blockEndPPQ)
            {
                const int sampleOffset = (int) std::round ((noteOffPPQ - blockStartPPQ) / ppqPerSample);
                midiMessages.addEvent (juce::MidiMessage::noteOff (1, 60),
                                       juce::jlimit (0, buffer.getNumSamples() - 1, sampleOffset));
            }
        }
    }

   #if JUCE_DEBUG
    {
        const int eventsGenerated = midiMessages.getNumEvents();
        DBG ("Eucl_Tito: MIDI events this block = " << eventsGenerated);

        juce::MidiBuffer::Iterator iterator (midiMessages);
        juce::MidiMessage message;
        int samplePosition = 0;

        while (iterator.getNextEvent (message, samplePosition))
        {
            const auto typeDescription = message.isNoteOn()  ? "noteOn"
                                         : message.isNoteOff() ? "noteOff"
                                                               : "other";
            DBG ("  -> " << typeDescription
                 << " note=" << message.getNoteNumber()
                 << " vel=" << (int) message.getVelocity()
                 << " sampleOffset=" << samplePosition);
        }
    }
   #endif
}

//==============================================================================
bool Eucl_TitoAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor* Eucl_TitoAudioProcessor::createEditor()
{
    return nullptr;
}

//==============================================================================
void Eucl_TitoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Eucl_TitoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Eucl_TitoAudioProcessor();
}
