/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class Eucl_TitoAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    Eucl_TitoAudioProcessor();
    ~Eucl_TitoAudioProcessor() override;

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

    // Step 1: Hard-coded pattern definition only.
    static constexpr int kNumSteps = 16;

private:
    std::array<uint8_t, kNumSteps> binaryPattern { 1, 0, 1, 0, 1, 0, 1, 0,
                                                   1, 0, 1, 0, 1, 0, 1, 0 };
    double localClockPpq { 0.0 };                   // Step 4 fallback clock
    double lastHostPpq { 0.0 };
    bool hostPositionInitialised { false };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Eucl_TitoAudioProcessor)
};
