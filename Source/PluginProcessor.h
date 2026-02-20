/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Juce::AudioProcessor subclass that owns the Euclidean sequencer logic.
    It sits in between the host (DAW) and the UI editor:
      * The host drives lifecycle calls such as prepareToPlay/processBlock.
      * processBlock emits MIDI events that the user interface (PluginEditor)
        can visualise or control once parameters are added.
*/
class Eucl_TitoAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    // Construction/Destruction – JUCE instantiates this via createPluginFilter().
    Eucl_TitoAudioProcessor();
    ~Eucl_TitoAudioProcessor() override;

    //==============================================================================
    // Host lifecycle hooks – prepare, release, validate busing layouts.
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    // Core audio/MIDI loop – the DAW calls this for each processing block.
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    // UI attachment – advertises whether a GUI exists and creates it on demand.
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    // Host capability queries – let the DAW know about MIDI/tail/program info.
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

    // Step 1: Hard-coded pattern definition only.
    static constexpr int kNumSteps = 16;

private:
    // Binary Euclidean sequence that drives the note generator in processBlock().
    std::array<uint8_t, kNumSteps> binaryPattern { 1, 0, 1, 0, 1, 0, 1, 0,
                                                   1, 0, 1, 0, 1, 0, 1, 0 };
    // Local transport bookkeeping so the sequencer can keep ticking if the host
    // provides no timeline data (or to smooth out sudden jumps).
    double localClockPpq { 0.0 };                   // Step 4 fallback clock
    double lastHostPpq { 0.0 };
    bool hostPositionInitialised { false };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Eucl_TitoAudioProcessor)
};
