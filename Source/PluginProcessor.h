/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Subclase de juce::AudioProcessor que contiene la lógica del secuenciador euclidiano.
    Se ubica entre el host (DAW) y el editor de UI:
      * El host ejecuta llamadas de ciclo de vida como prepareToPlay/processBlock.
      * processBlock emite eventos MIDI que la interfaz (PluginEditor) podrá
        visualizar o controlar una vez que se añadan parámetros.
*/
class Eucl_TitoAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    // Construcción/Destrucción – JUCE instancia esta clase mediante createPluginFilter().
    Eucl_TitoAudioProcessor();
    ~Eucl_TitoAudioProcessor() override;

    //==============================================================================
    // Hooks del ciclo de vida del host – preparar, liberar y validar configuraciones de buses.
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    // Loop principal de audio/MIDI – el DAW invoca esto para cada bloque de procesamiento.
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    // Enlace con la UI – informa si existe GUI y la crea bajo demanda.
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    // Consultas de capacidades – informa al DAW sobre MIDI/tail/program info.
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

    // Paso 1: definición del patrón solamente, codificada de forma fija.
    static constexpr int kNumSteps = 16;

private:
    // Secuencia binaria euclidiana que alimenta al generador de notas en processBlock().
    std::array<uint8_t, kNumSteps> binaryPattern { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    // Control del transporte local para que el secuenciador siga corriendo si el host
    // no entrega datos de timeline (o para suavizar saltos bruscos).
    double localClockPpq { 0.0 };                   // Paso 4: reloj de respaldo
    double lastHostPpq { 0.0 };
    bool hostPositionInitialised { false };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Eucl_TitoAudioProcessor)
};
