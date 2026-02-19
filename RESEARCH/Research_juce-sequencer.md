# Research: JUCE Sequencer Implementation
- **Author:** Cascade Agent
- **Date:** 2026-02-17
- **Audience:** Solo JUCE developer building a MIDI step/EUCL sequencer plug-in
- **Goal:** Provide proven patterns for modelling, scheduling, and rendering a tempo-synchronised sequencer in JUCE
- **Confidence:** Medium-High – official docs/tutorials plus JUCE forum guidance and working OSS example

## Scope & Questions
1. How should sequencer data structures be organised (model vs. processor) to stay real-time safe while supporting editing?
2. Which JUCE timing primitives (AudioPlayHead, sample counting, timers) best keep sequencer playback aligned with host/standalone transport?
3. How are MIDI events scheduled per block without introducing allocations or race conditions?
4. What UI + parameter patterns keep the editor in sync with audio output?

## Source Log
| Type | Reference | Date | Notes |
| --- | --- | --- | --- |
| Internal research | [Research_euclidean-sequencer-in-juce.md](./Research_euclidean-sequencer-in-juce.md) | 2026-02-13 | Prior architecture & timing findings specific to Euclidean patterns. |
| Internal notes | [Resources.md](./Resources.md) | 2026-02-17 | Collected links + outline of Sequencer core components. |
| Official docs | [juce::MidiMessageSequence](https://docs.juce.com/master/classjuce_1_1MidiMessageSequence.html) | 2026-02-17 | Timestamped MIDI clip container, addEvent semantics, iterators. |
| Official docs | [juce::AudioPlayHead](https://docs.juce.com/master/classjuce_1_1AudioPlayHead.html) | 2026-02-17 | Host transport info, `CurrentPositionInfo` contract. |
| Official tutorial | [Create MIDI data](https://juce.com/tutorials/tutorial_midi_message) | 2026-02-17 | MidiMessage factories, MidiBuffer iteration patterns. |
| JUCE Forum | [(Beginner) MIDI Sequencing](https://forum.juce.com/t/beginner-midi-sequencing/61886) | 2026-02-17 | Deriving sequencer phase from AudioPlayHead per block. |
| JUCE Forum | [MIDI Sequencer design: data structures and synchronisation](https://forum.juce.com/t/midi-sequencer-design-data-structures-and-synchronization/65967) | 2026-02-17 | Double-buffer MidiMessageSequence swap, FIFO command queue. |
| GitHub | [Klide – Euclidean rhythm VST3](https://github.com/dmultiply/Klide) | 2026-02-17 | Reference multi-track Euclidean implementation (patterns, UI). |

## Key Findings
1. **Separate editor-friendly models from audio-thread state and publish immutable snapshots.** Keep note data (e.g., per-step structs) on the GUI side, regenerate a `juce::MidiMessageSequence` snapshot off-thread, then swap into the processor via a double buffer guarded only by atomics/FIFO commands.[1][2][7]
2. **Use `AudioPlayHead::getCurrentPosition()` to derive PPQ/sample phase once per block.** Convert BPM + sample rate into `ppqPerSample`, advance a phase accumulator, and drive the sequencer even when the host transport stops by falling back to a `HighResolutionTimer` clock.[1][4][6]
3. **Schedule MIDI with pre-built clips plus per-block gate handling.** Store note-on/off timestamps inside `MidiMessageSequence`, then inside `processBlock` copy only the events whose timestamps fall within the current block into a `MidiBuffer`, ensuring no allocations in the real-time path.[1][3][5]
4. **Expose sequencer parameters through `AudioProcessorValueTreeState` and rebuild patterns deterministically.** Parameters such as steps, hits, rotation, velocity, gate, swing should trigger regeneration callbacks that enqueue new snapshots; this mirrors the pattern used in Klide for multiple Euclidean lanes.[1][2][8]
5. **UI grids should render directly from the published snapshot and highlight the transport-driven step index.** Paint from the immutable pattern vector, and trigger `repaint` either when parameters change or when the audio thread advances the highlighted step (communicated via atomics or timer callbacks).[1][2][8]

## Implementation / Application Notes
- **Data model:** Define lightweight `struct Step { uint8_t note; uint8_t velocity; bool gate; }` arrays per track on the GUI side. When parameters change, rebuild a `MidiMessageSequence` with paired on/off events positioned in PPQ, then push to processor via lock-free FIFO.[2][3][7]
- **Timing math:** In `processBlock`, query `AudioPlayHead` -> `CurrentPositionInfo` to extract `ppqPosition`, `bpm`, and `timeInSamples`. Compute `samplesPerBeat = sampleRate * 60 / bpm` and derive samples per step or PPQ offsets for Euclidean distributions.[4][6]
- **Scheduling loop:** For each block, determine `blockStartPpq` and `blockEndPpq`. Iterate the cached sequence via `MidiMessageSequence::getNextIndexAtTime` and add events in-range to the outgoing `MidiBuffer` using the offsets returned by the iterator. Use `MidiMessage::noteOn/noteOff` helpers for adhoc events (e.g., audition buttons).[3][5]
- **Transport fallback:** If `getCurrentPosition()` returns false (standalone), drive tempo via `HighResolutionTimer` or `juce::AudioTransportSource`, updating the same phase accumulator used for host sync so behaviour is identical.[2][6]
- **Thread-safe updates:** Use `juce::AbstractFifo` or a custom single-producer/single-consumer queue so the GUI can enqueue "swap buffer" commands; the audio thread reads commands at block start, swaps the active `MidiMessageSequence*`, and proceeds without locking.[7]
- **UI techniques:** Represent the grid with a custom `Component` that caches rectangles per step. Use lambdas or `std::function` callbacks supplied by the processor/editor bridge to toggle steps, regenerate patterns, and request repaint. Reference Klide’s `TrackComponent` for multi-lane layouts.[2][8]

## Step-by-Step: Sequencer for a Binary Pattern
1. **Declare the pattern source.** Store the binary array plus double-buffered sequences inside the processor so both threads can read immutable snapshots. Add the following in `Source/PluginProcessor.h` after the private section comment (around line 56):

   ```cpp
   // File: Source/PluginProcessor.h (line ~56)
   static constexpr int kNumSteps = 16;
   std::array<uint8_t, kNumSteps> binaryPattern { 1, 0, 1, 0, 1, 0, 1, 0,
                                                  1, 0, 1, 0, 1, 0, 1, 0 };
   std::array<juce::MidiMessageSequence, 2> patternSequences;
   std::atomic<int> activeSequence { 0 };
   juce::AbstractFifo swapFifo { 8 };
   ```

2. **Precompute note events.** Create a helper that rebuilds the inactive `MidiMessageSequence` whenever the pattern changes. Place this in `Source/PluginProcessor.cpp` near the other private helpers (around line 110):

   ```cpp
   // File: Source/PluginProcessor.cpp (line ~110)
   void Eucl_TitoAudioProcessor::rebuildPatternSequence()
   {
       const double stepLengthPPQ = 0.25; // quarter-note subdivision
       const double gateLengthPPQ = 0.10;

       const int inactive = 1 - activeSequence.load();
       auto& sequence = patternSequences[(size_t) inactive];
       sequence.clear();

       for (int i = 0; i < kNumSteps; ++i)
       {
           if (binaryPattern[(size_t) i] == 0)
               continue;

           const double start = i * stepLengthPPQ;
           sequence.addEvent (juce::MidiMessage::noteOn  (1, 60, (juce::uint8) 100), start);
           sequence.addEvent (juce::MidiMessage::noteOff (1, 60), start + gateLengthPPQ);
       }

       swapFifo.write (1);
   }
   ```

3. **Swap sequences safely.** Drain the FIFO inside `processBlock` before rendering so the audio thread only flips pointers, never mutates data. Insert this helper call near the start of `processBlock` (around line 136 in `Source/PluginProcessor.cpp`):

   ```cpp
   // File: Source/PluginProcessor.cpp (line ~136)
   auto drainSwapFifo = [this]() noexcept
   {
       while (swapFifo.getNumReady() > 0)
       {
           swapFifo.read (1);
           activeSequence.store (1 - activeSequence.load());
       }
   };

   drainSwapFifo();
   ```

4. **Derive timing per block.** Query the playhead once per call and compute PPQ-to-sample ratios. Add the snippet below near the top of `processBlock` after draining the FIFO (around line 150):

   ```cpp
   // File: Source/PluginProcessor.cpp (line ~150)
   juce::AudioPlayHead::CurrentPositionInfo posInfo;
   const bool hasPosition = getPlayHead() != nullptr && getPlayHead()->getCurrentPosition (posInfo);

   const double bpm = hasPosition && posInfo.bpm > 0.0 ? posInfo.bpm : 120.0;
   const double samplesPerBeat = getSampleRate() * 60.0 / bpm;
   const double ppqPerSample = 1.0 / samplesPerBeat;
   const double blockStartPPQ = hasPosition ? posInfo.ppqPosition : localClockPpq;
   const double blockEndPPQ   = blockStartPPQ + (buffer.getNumSamples() * ppqPerSample);
   ```

5. **Emit MIDI from the active sequence.** Walk the active sequence and copy events that fall inside the current block into the outgoing `MidiBuffer`. Append this loop after the timing math (line ~165 in `Source/PluginProcessor.cpp`):

   ```cpp
   // File: Source/PluginProcessor.cpp (line ~165)
   auto& sequence = patternSequences[(size_t) activeSequence.load()];
   const int startIndex = sequence.getNextIndexAtTime (blockStartPPQ);

   for (int i = startIndex; i < sequence.getNumEvents(); ++i)
   {
       const auto* holder = sequence.getEventPointer (i);
       const double eventTime = holder->message.getTimeStamp();
       if (eventTime >= blockEndPPQ)
           break;

       const int sampleOffset = int ((eventTime - blockStartPPQ) / ppqPerSample);
       midiMessages.addEvent (holder->message, juce::jlimit (0, buffer.getNumSamples() - 1, sampleOffset));
   }
   ```

6. **Keep UI feedback in sync.** Publish the currently playing step via an atomic so the editor can repaint. Update a member before exiting `processBlock` (around line 185) and read it in `PluginEditor.cpp` when painting the grid:

   ```cpp
   // File: Source/PluginProcessor.cpp (line ~185)
   currentStep.store ((int) std::floor (std::fmod (blockStartPPQ / 0.25, kNumSteps)));
   ```

   ```cpp
   // File: Source/PluginEditor.cpp (line ~60)
   const auto playingStep = audioProcessor.getCurrentStep();
   for (int i = 0; i < Eucl_TitoAudioProcessor::kNumSteps; ++i)
   {
       const bool isActive = audioProcessor.isStepActive (i);
       const bool isPlaying = (i == playingStep);
       g.setColour (isPlaying ? juce::Colours::orange : (isActive ? juce::Colours::white : juce::Colours::dimgrey));
       g.fillRect (stepArea);
       stepArea.translate (stepWidth + padding, 0);
   }
   ```

7. **Expose controls to load or edit arrays.** Provide a text box or toggle buttons that modify `binaryPattern`, then call `rebuildPatternSequence()`. Add the following handler in `PluginEditor.cpp` near your UI callbacks (around line 90):

   ```cpp
   // File: Source/PluginEditor.cpp (line ~90)
   void Eucl_TitoAudioProcessorEditor::onPatternTextChanged (const juce::String& bits)
   {
       std::array<uint8_t, Eucl_TitoAudioProcessor::kNumSteps> newPattern {};
       for (int i = 0; i < newPattern.size(); ++i)
           newPattern[(size_t) i] = (uint8_t) (bits[i] == '1');

       audioProcessor.setBinaryPattern (newPattern);
   }
   ```

   Inside `setBinaryPattern` (add in `PluginProcessor.cpp` around line 210) copy the array, call `rebuildPatternSequence()`, and let the FIFO trigger the swap.

## Risks & Pitfalls
- **Allocations in `processBlock`:** Avoid calling `addEvent` or modifying STL containers on the audio thread. Pre-build sequences and reuse `MidiBuffer` objects per block.[3][7]
- **Missing playhead data:** Some hosts (and standalone mode) may not populate `CurrentPositionInfo`. Always guard the result of `getCurrentPosition()` and provide an internal clock fallback.[4][6]
- **GUI/audio desync:** Sharing mutable containers between threads can cause tearing or crashes. Rely on copy-on-publish snapshots plus atomics for lightweight flags (e.g., current step index).[1][7]
- **UI performance:** Repainting the entire grid on every timer tick can saturate the message thread. Only repaint cells that change (e.g., current step highlight) or throttle repaint frequency.[2][8]

## Open Questions & Next Steps
- Decide whether per-step modulation (velocity, probability, ratchets) is required and extend the snapshot schema accordingly.
- Prototype the FIFO swap mechanism with `juce::AbstractFifo` and stress-test rapid parameter drags to ensure no missed updates.
- Explore integrating an internal synth voice (as Klide does) vs. emitting pure MIDI, and document the MIDI routing expectations for users.
- Validate tempo-swing or shuffle support: define how swing affects PPQ timestamps before committing to UI controls.
