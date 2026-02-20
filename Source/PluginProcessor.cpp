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
    // Configuración del grid del secuenciador expresada en pulsos por negra (PPQ).
    // El procesador y el editor podrán exponer estos valores como parámetros más adelante;
    // por ahora se comportan como los objetos "Step" y "Gate" en un patch de Max.
    constexpr double kStepLengthPPQ = 0.25; // negra subdividida en corcheas
    constexpr double kGateLengthPPQ = 0.10; // valores menores acortan la nota
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
    // El constructor se mantiene liviano a propósito; el trabajo pesado ocurre en
    // prepareToPlay() una vez que el host entrega sample rate y tamaño de buffer.
}

Eucl_TitoAudioProcessor::~Eucl_TitoAudioProcessor()
{
    // Aún no hay nada que liberar; marcador para futuras asignaciones (buffers,
    // attachments de parámetros) que seguirán el ciclo prepareToPlay()/releaseResources().
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
    return 1;   // Nota: algunos hosts no manejan bien que existan 0 programas,
                // por lo que conviene informar al menos 1 aunque no se usen presets.
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
    // Hook que se ejecuta justo antes de que comience el audio. En términos de Max,
    // este sería el lugar para preparar objetos metro/clock; aquí reiniciaremos
    // acumuladores de fase o asignaremos buffers según la información temporal del host.
}

void Eucl_TitoAudioProcessor::releaseResources()
{
    // Contraparte simétrica de prepareToPlay(); libera buffers o recursos sueltos
    // para mantener liviano al procesador cuando el host está inactivo.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Eucl_TitoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Acá es donde se valida si el diseño de buses es compatible.
    // En esta plantilla solo se aceptan mono o estéreo.
    // Algunos hosts (p. ej. ciertas versiones de GarageBand) únicamente cargan
    // plugins que soporten buses estéreo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Verifica que la configuración de entrada coincida con la de salida
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
    // Este plugin solo emite MIDI gates, así que mantenemos el buffer de audio en silencio
    // para evitar enviar DC o basura al mezclador del host (similar a dejar un cable MSP~ suelto
    // mientras se sigue usando la red de eventos en Max).
    buffer.clear();

    midiMessages.clear();

    const double sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return;

    // Paso 4: derivar la sincronía desde el playhead del host si está disponible; de lo contrario,
    // avanzar con nuestro reloj interno. Este es el "cerebro" de transporte: escucha el playhead
    // del DAW (similar a conducir un patch con Ableton Link) y, si no hay datos, recurre a un contador
    // interno para que el secuenciador continúe.
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

    // Paso 5: emitir eventos MIDI cuyo rango caiga dentro del bloque actual (patrón en loop).
    // Conceptualmente, cada iteración es como un [uzi] de Max disparando notas hacia un
    // [makenote]: recorremos el patrón, creamos pares note-on/off y los escribimos en el
    // MidiBuffer para que el host escuche el groove en la muestra correcta.
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

        // El output opcional del debug funciona como enviar la lista MIDI a un print de Max
        // para inspeccionar el flujo de notas desde la consola.
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
    // El procesador sigue sin interfaz; devolver false evita que el host pida un editor
    // hasta que implementemos una UI real.
    return false;
}

juce::AudioProcessorEditor* Eucl_TitoAudioProcessor::createEditor()
{
    // Marcador temporal: cuando esté listo instanciará Eucl_TitoAudioProcessorEditor
    // para que el usuario pueda manipular el secuenciador desde la UI.
    return nullptr;
}

//==============================================================================
void Eucl_TitoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
    // Futuro hogar para la serialización de presets (como guardar un patch de Max):
    // almacenará el árbol de parámetros/valores para que el DAW restaure el patrón al abrir la sesión.
}

void Eucl_TitoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
    // Espejo de getStateInformation(); aplica el estado guardado a los parámetros
    // para que el secuenciador continúe exactamente donde lo dejó el usuario.
}

//==============================================================================
// Crea nuevas instancias del plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Eucl_TitoAudioProcessor();
}
