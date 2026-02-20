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
    // Sequencer grid configuration expressed in pulses-per-quarter (PPQ).
    // The processor and editor can later expose these as parameters; for now they
    // act like the "Step" and "Gate" objects in a Max patch.
    constexpr double kStepLengthPPQ = 0.25; // quarter note subdivided into 16th notes
    constexpr double kGateLengthPPQ = 0.10; // smaller value shortens note length
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
    // Constructor is intentionally light-weight; heavy lifting belongs to
    // prepareToPlay() once the host provides sample rate and buffer size.
}

Eucl_TitoAudioProcessor::~Eucl_TitoAudioProcessor()
{
    // Nothing to tear down yet; placeholder for future allocations (i.e. buffers,
    // parameter attachments) that will mirror prepareToPlay()/releaseResources().
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
    juce::ignoreUnused (sampleRate, samplesPerBlock);
    // Hook called right before audio starts. In Max terms this is where you would
    // prime your metro/clock objects; here we will later reset phase accumulators
    // or allocate buffers that depend on the host's timing info.
}

void Eucl_TitoAudioProcessor::releaseResources()
{
    // Symmetric counterpart to prepareToPlay(); free buffers or detach resources
    // so the processor stays lightweight when the host is idle.
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
    // This plug-in only emits MIDI gates, so we keep the audio buffer silent to
    // avoid feeding DC/garbage to the host's mixer (akin to leaving an MSP~ cord
    // disconnected while still using Max's event network).
    buffer.clear();

    midiMessages.clear();

    const double sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return;

    // Step 4: Derive timing from host playhead if available, otherwise advance our own clock.
    // This is the processor's transport brain: it listens to the DAW playhead
    // (similar to driving a patch with Ableton Link) and, if unavailable, falls
    // back to an internal counter so the sequencer still runs.
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
    // Conceptually, each iteration is like a Max [uzi] sending note triggers to a
    // [makenote] object: we walk the pattern, create note-on/off pairs, and stamp
    // them into the MidiBuffer so the host hears the groove at the right sample.
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

        // Optional debug dump behaves like piping the MIDI list into Max's print
        // object so you can inspect the note stream in the console.
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
    // The processor is currently headless; returning false ensures the host does
    // not request an editor until we provide a proper UI implementation.
    return false;
}

juce::AudioProcessorEditor* Eucl_TitoAudioProcessor::createEditor()
{
    // Placeholder – once ready, instantiate Eucl_TitoAudioProcessorEditor so the
    // user can tweak the sequencer visually.
    return nullptr;
}

//==============================================================================
void Eucl_TitoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
    // Future home for preset serialization (akin to saving a Max patch): stash
    // parameter/value tree state so the DAW restores the pattern on session load.
}

void Eucl_TitoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
    // Mirror of getStateInformation(); apply the saved state back to parameters
    // so the sequencer picks up exactly where the user left off.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Eucl_TitoAudioProcessor();
}
