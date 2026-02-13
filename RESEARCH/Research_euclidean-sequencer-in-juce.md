# Research: Euclidean Sequencer in JUCE
- **Author:** Cascade Agent
- **Date:** 2026-02-13
- **Audience:** Solo JUCE developer planning a MIDI sequencer plugin
- **Goal:** Establish architecture, timing, and UI patterns to implement a Euclidean (Bjorklund) sequencer in JUCE
- **Confidence:** Medium – mix of official docs/tutorials plus validated JUCE forum guidance

## Scope & Questions
1. How should Euclidean rhythm generation be structured and parameterised for reuse across voices?
2. What JUCE timing primitives (AudioPlayHead, sample counting, timers) best keep the sequencer locked to host transport and standalone clocks?
3. Which data structures and threading patterns keep GUI editing and audio rendering real-time safe?
4. How should the UI surface hits/steps/rotation while staying in sync with underlying parameters and MIDI output?

## Source Log
| Type | Reference | Date | Notes |
| --- | --- | --- | --- |
| Official docs | [juce::MidiMessageSequence](https://docs.juce.com/master/classjuce_1_1MidiMessageSequence.html) | 2026-02-13 | Timestamped storage + utilities for MIDI clips/events | 
| Official docs | [juce::AudioPlayHead](https://docs.juce.com/master/classjuce_1_1AudioPlayHead.html) | 2026-02-13 | Host transport/BPM/PPQ data for processBlock synchronisation |
| Official tutorial | [Create MIDI data (MidiMessage + MidiBuffer)](https://juce.com/tutorials/tutorial_midi_message) | 2026-02-13 | Refresh on crafting/scheduling MIDI messages |
| JUCE Forum | [Midi Sequencer design: data structures and synchronization](https://forum.juce.com/t/midi-sequencer-design-data-structures-and-synchronization/65967) | 2026-02-13 | Double-buffered MidiMessageSequence syncing pattern |
| JUCE Forum | [(Beginner) MIDI Sequencing](https://forum.juce.com/t/beginner-midi-sequencing/61886) | 2026-02-13 | Guidance to derive sequencer phase from AudioPlayHead |
| JUCE Forum | [Real time safe reading and writing of std::vector](https://forum.juce.com/t/real-time-safe-reading-and-writing-of-std-vector/43665) | 2026-02-13 | FIFO handoff between GUI edits and audio thread |
| GitHub | [Klide – JUCE Euclidean rhythm plugin](https://github.com/dmultiply/Klide) | 2026-02-13 | Reference implementation (multi-track Euclidean sampler) |

## Key Findings
1. **Model Euclidean patterns as immutable bitsets regenerated on parameter edits.** A lightweight generator (Bjorklund algorithm) can output `std::vector<bool>` per voice, and sequences of `juce::MidiMessage` objects can be rebuilt into `juce::MidiMessageSequence` whenever steps/hits/rotation change, keeping audio-thread work to simple index lookups.[1][4]
2. **Synchronise playback via `AudioPlayHead::getCurrentPosition()` to derive PPQ/sample offsets each block.** Calculating `samplesPerStep = (sampleRate * 60) / (bpm * stepsPerBeat)` and advancing a phase accumulator keeps sequencer timing aligned whether the host transport is running or not; standalone builds can fall back to `HighResolutionTimer` or `juce::AudioTransportSource`.[2][5]
3. **Use double-buffered MIDI data and lock-free FIFOs for GUI→audio updates.** Editors enqueue commands (pattern rebuilds, parameter snapshots) to a FIFO; the audio thread swaps in the prepared `MidiMessageSequence` copy at block boundaries, avoiding reallocations or locks in `processBlock`.[1][4][6]
4. **Expose parameters through `AudioProcessorValueTreeState` so GUI widgets and automation rebuild patterns deterministically.** Parameters such as `steps`, `hits`, `rotation`, `velocity`, and `note` should trigger regeneration callbacks that push new pattern data through the FIFO, mirroring how mature plugins like Klide multiplex several Euclidean lanes.[3][6][7]
5. **UI grids should render from canonical pattern data rather than recomputing state.** A custom `Component` can paint columns for each step, with hit states pulled from the processor’s immutable pattern snapshot; repaint requests should follow parameter changes to guarantee visual/audio parity.[4][7]

## Implementation / Application Notes
- **Core generator:** Implement Bjorklund-style distribution (e.g., recursive bucket or iterative remainder algorithm) returning a `std::array<bool, maxSteps>` to avoid heap churn. Clamp `hits` to `[0, steps]` and provide rotation via `std::rotate` on the cached buffer before publishing.[4]
- **Parameter block:** Define `AudioParameterInt steps (1–64)`, `hits (0–64)`, `rotation (0–63)`, `note (0–127)`, `velocity (1–127)`, plus `AudioParameterFloat` for decay/accent. Attach listeners that trigger pattern regeneration and enqueue the result.[3][6]
- **Timing math:** Inside `processBlock`, query `AudioPlayHead` once per block; derive `double ppqPosStart` and `ppqPerSample = bpm / (60.0 * sampleRate)`. Convert desired steps-per-bar into PPQ spans to know when step boundaries occur as samples advance.[2][5]
- **MIDI scheduling:** Maintain a small `MidiBuffer` per block; whenever the phase crosses a step boundary and `pattern[currentStep]` is true, insert paired `noteOn`/`noteOff` messages offset by `gateLengthSamples`. Use `MidiMessage::noteOn`/`noteOff` factory helpers from the tutorial.[1][3]
- **Transport awareness:** Skip advancement when `positionInfo.isPlaying` is false but still allow audition by exposing a stand-alone clock toggled via UI (use `HighResolutionTimer` to tick every `stepDurationMs`).[2][5]
- **Thread safety:** Store pattern/state copies in the processor. GUI components read via atomics or `AudioProcessorValueTreeState` attachments; heavy operations (vector rebuilds) happen off the audio thread and the result is published via FIFO swap, as recommended by forum guidance.[4][6]
- **Extensibility:** For multi-track Euclidean sequencers (Klide), manage a `std::array<PatternState, numTracks>` and schedule per-track MIDI channels or note numbers, sharing the same timing accumulator but independent rotation/offset parameters.[7]

## Risks & Pitfalls
- **Heap allocations in audio thread:** Avoid `std::vector::push_back` or `MidiMessageSequence::addEvent` inside `processBlock`. Pre-build sequences or cache pattern arrays to keep realtime code allocation-free.[1][4]
- **Transport desync:** Hosts may omit playhead data (e.g., standalone). Provide graceful fallback clocks and guard against `AudioPlayHead::getCurrentPosition()` returning `false`.[2]
- **Data races between GUI edits and audio thread:** Refrain from sharing mutable containers directly; use lock-free command queues or parameter attachments to convey updates safely.[4][6]
- **UI lag:** Repainting complex grids each timer tick can starve message thread. Batch updates (e.g., `repaint` only when parameters change or when the highlighted step moves) and use cached bitmaps where possible.[7]

## Open Questions & Next Steps
- Validate whether the project needs per-step velocity/accent and how to store that without breaking real-time constraints (potentially struct-of-arrays per track).
- Decide on the MIDI output contract (single channel drum map vs. per-track channels) and add host automation names accordingly.
- Prototype the FIFO swap implementation (e.g., `AbstractFifo`) and unit-test for glitch-free updates when rapidly dragging parameters.
- Explore integrating sample playback or internal synth voices similar to Klide to reduce external routing required for users.[7]
