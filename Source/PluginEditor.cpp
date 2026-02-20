/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Eucl_TitoAudioProcessorEditor::Eucl_TitoAudioProcessorEditor (Eucl_TitoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Max analogy: this component is the presentation frame; setSize() defines
    // the canvas where knobs, toggles, and sequencer lights will live.
    setSize (400, 300);
}

Eucl_TitoAudioProcessorEditor::~Eucl_TitoAudioProcessorEditor()
{
}

//==============================================================================
void Eucl_TitoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // paint() is equivalent to Max's front-end drawing: repaint the background
    // and text for every frame Juce requests (e.g., after a resize or parameter change).
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void Eucl_TitoAudioProcessorEditor::resized()
{
    // Layout callback – wire subcomponents here once the UI grows. Treat it like
    // arranging UI boxes in Max so all parameter widgets talk to audioProcessor.
}
