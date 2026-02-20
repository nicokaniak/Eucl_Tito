/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    Minimal GUI shell that will eventually expose the sequencer controls.
    Think of it as the Max "presentation view" talking to the processor:
      * Holds a reference to Eucl_TitoAudioProcessor so it can read/write params.
      * Overrides paint()/resized() to draw widgets once they exist.
*/
class Eucl_TitoAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    Eucl_TitoAudioProcessorEditor (Eucl_TitoAudioProcessor&);
    ~Eucl_TitoAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Direct handle to the processor – comparable to patch cords between UI
    // sliders and the underlying Max objects so parameter changes flow both ways.
    Eucl_TitoAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Eucl_TitoAudioProcessorEditor)
};
