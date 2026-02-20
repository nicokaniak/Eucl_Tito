# Eucl_Tito Sequencer Patch Map (Max/MSP Analogy)

This document explains how the JUCE classes inside the `Source` folder correspond to familiar Max/MSP patch elements. Treat each section as a module in a virtual patch bay and follow the signal flow arrows to understand how timing data and events travel through the system.

## 1. Cast of Objects

| JUCE Class/Member | Max/MSP Analogy | Purpose | Talks To |
|------------------|-----------------|---------|----------|
| `Eucl_TitoAudioProcessor` | **[patcher]** main subpatch | Owns the Euclidean sequencer brain, communicates with host and editor | Host transport, `Eucl_TitoAudioProcessorEditor` |
| `processBlock()` | **[metro]/[uzi] + logic** | Clocked loop that reads the binary pattern and injects MIDI into the host | `binaryPattern`, `juce::MidiBuffer` |
| `binaryPattern` | **[coll] / table** | Hard-coded 0/1 list describing which steps fire | `processBlock()` |
| `localClockPpq`, `lastHostPpq` | **[phasor~]/timer** | Keeps time when host data is missing or jumps | `processBlock()` |
| `Eucl_TitoAudioProcessorEditor` | **Presentation view UI** | Future visual step grid that edits processor parameters | `Eucl_TitoAudioProcessor` |
| `juce::MidiBuffer` | **[makenote] output cord** | Collects timestamped note-on/off messages emitted each block | Host DAW |

## 2. Signal Flow Overview

```
Host Transport --> processBlock() --> Binary Pattern Lookup --> MIDI Buffer --> DAW Instrument Track
                    ^                     |
                    |                     v
             Editor (future UI) <--> Eucl_TitoAudioProcessor
```

1. **Host Transport** provides tempo/BPM and the playhead position.
2. `processBlock()` converts playhead info into pulses-per-quarter (PPQ) increments.
3. The **Binary Pattern Lookup** checks which Euclidean steps are active and schedules note-on/off events.
4. Events accumulate inside `juce::MidiBuffer`, similar to Max's event list, and the host consumes them on the next audio block.
5. Once implemented, the **Editor UI** will tweak parameters by talking directly to the processor (two-way cord).

## 3. Section-by-Section Tour

### A. Transport Intake (`processBlock` lines 146–188)
- **What:** Reads host tempo and playhead and maintains a fallback local clock.
- **Why:** Ensures the sequencer remains locked to the DAW; if transport data is missing, the internal PPQ clock keeps ticking so you still hear a groove.
- **Connection:** Output PPQ positions feed the loop/pattern iterator.

### B. Euclidean Loop Engine (`processBlock` lines 190–226)
- **What:** Iterates over each step within the current audio block, checks `binaryPattern`, and schedules note-on/off.
- **Why:** Breaks the continuous transport timeline into repeated Euclidean cycles, analogous to a Max `[uzi]` hitting `[select]` objects for active steps.
- **Connection:** Feeds the MIDI buffer, which is the actual connection to the host's synths.

### C. Debug Tap (`#if JUCE_DEBUG` block 228–250)
- **What:** Dumps generated MIDI events to the console during development.
- **Why:** Mirrors sending a patch cord into Max's `[print]` so you can verify timing and step density.
- **Connection:** Non-destructive listener that reads from the same MIDI buffer destined for the host.

### D. Editor Shell (PluginEditor.*)
- **What:** Simple component that currently paints "Hello World" but is wired to the processor.
- **Why:** Serves as the placeholder GUI where sliders/buttons will eventually change the sequencer parameters.
- **Connection:** Holds a reference to the processor exactly like a UI abstraction connected to a subpatch in Max; future UI actions will call processor methods.

## 4. Future Patch Cords to Add

1. **Parameter ValueTree** – acts like `[pattrstorage]` so presets can save the pattern.
2. **MIDI Channel/Note Controls** – equivalent to exposing `[number]` boxes on the UI.
3. **Gate/Step Controls** – convert `kStepLengthPPQ` and `kGateLengthPPQ` constants into editable parameters with smoothing.

Keeping this mental map in mind should make it easier to extend the project: whenever you add a feature, decide which side of the patch (transport, pattern math, MIDI emit, or UI) it belongs to, and connect it using the same JUCE class relationships described above.
