https://www.youtube.com/watch?v=tgf6J8foCiw
https://juce.com/tutorials/tutorial_midi_message/
https://forum.juce.com/t/beginner-midi-sequencing/61886
https://github.com/dmultiply/Klide
https://forum.juce.com/t/midi-sequencer-where-to-beggin/57938
https://forum.juce.com/t/midi-sequencer-design-data-structures-and-synchronization/65967
https://docs.juce.com/master/classjuce_1_1MidiMessageSequence.html

-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# SEQUENCER
(Use MidiMessageSequence for event storage/playback—it's JUCE's built-in sequencer primitive. Pair with AudioProcessor/Player for timing via getNextAudioBlock. Drive via TransportSource or HighResolutionTimer for tempo/BPM control. UI: Custom Component with TableListBox or painted grid for steps.)
Key Components:
SequenceModel: Owns MidiMessageSequence, step grid (e.g., Array<Array<bool>> for triggers, notes/velocities).
SequencerEngine: Inherits AudioSource; renders MIDI in real-time using sample-accurate timestamps.
TempoControl: HighResolutionTimer or callback-driven; advances playhead via PPQ (pulses per quarter) math.
Implementation Steps
Model Setup (SequenceModel.h/cpp):
text
class SequenceModel {
    MidiMessageSequence sequence;
    double bpm = 120.0, ppqPos = 0.0; // Playhead
    Array<int> steps; // e.g., 16 steps x notes
    void addStep(int step, int note, int vel) {
        double time = (step / 16.0) * (60.0 / bpm * ppqPerBar);
        sequence.addEvent(MidiMessage::noteOn(1, note, (float)vel), time);
    }
};
Audio Rendering (SequencerEngine : AudioSource):

text
void getNextAudioBlock(AudioSourceChannelInfo& info) override {
    MidiBuffer midiBuf;
    auto numSamples = info.numSamples;
    double samplesPerBeat = getSampleRate() * 60.0 / bpm;
    // Advance ppqPos by numSamples / samplesPerBeat * ppqPerBeat
    model.updatePosition(ppqPos, numSamples); // Fill midiBuf with events
    synth.renderNextBlock(info.buffer, midiBuf, 0, numSamples); // Forward to synth
}
Timer for Transport (in MainComponent):

text
class SequencerTimer : HighResolutionTimer {
    void hiResTimerCallback() override {
        auto elapsed = startTime.toMilliseconds();
        ppqPos = (elapsed / 1000.0) * (bpm / 60.0) * ppqPerBar;
        if (ppqPos >= sequenceLength) ppqPos = 0; // Loop
    }
};
UI Grid (StepComponent : Component):

Paint rectangles for steps; mouseDown toggles triggers.

Link to model via std::function callbacks.
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# EUCLIDEAN
Core Algorithm
Implement std::vector<bool> generateEuclidean(int steps, int hits):

text
vector<bool> generateEuclidean(int steps, int hits) {
    vector<bool> pattern(steps, false);
    int pos = 0;
    for (int i = 0; i < hits; ++i) {
        pattern[pos] = true;
        pos = (pos + steps / (hits - i)) % steps;
    }
    return pattern;
}
Handles hits > steps by capping at steps.

Rotation: Shift via std::rotate(pattern.begin(), pattern.begin() + offset, pattern.end()).

Validates: Test E(7,3) → 1001001; E(5,8) → 11111000 (prioritizes even spacing).

JUCE Integration Steps
AudioProcessor: In processBlock, track transport position (AudioPlayHead::CurrentPositionInfo). Advance step index via totalSamples % (sampleRate * stepsPerBeat / bpm).

Sequencer Class: class EuclideanSeq { int steps=16, hits=8, rotation=0, currentStep=0; void advance() { currentStep = (currentStep + 1) % steps; } bool isHit() { return pattern[currentStep]; } }; Regenerate pattern on param change.

Parameters: AudioParameterInt stepsParam("steps", "Steps", 1, 64); etc. Use paramChanged callback to rebuild.

MIDI Out: If isHit() and transport playing, sendNoteOn(MidiMessage::noteOn(...)).

UI: Slider for steps/hits/rotation; CustomComponent paints grid (rows=1 voice, cols=steps; fill if pattern[j]). Repaint on param drag.
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# ERRORS_HANDLING
- Core Approach
  JUCE favors juce::Result over exceptions for error propagation in most APIs, enabling lightweight, exception-free handling suitable for real-time audio. Exceptions are minimized to avoid performance hits and complexity in audio threads; forums advise against routine use. (https://forum.juce.com/t/juce-and-exception-handling/15144)
- Key Tools
              juce::Result: Signals success (Result::ok()) or failure with message (Result::fail("msg")). Check via if (result.ok()) or if (!result); access error with result.getErrorMessage(https://docs.juce.com/master/classjuce_1_1Result.html).
              Assertions: jassert() for debug checks (e.g., invalid states); fails loudly in dev builds. (https://www.reddit.com/r/JUCE/comments/1bmpp61/about_assertion_failure_error/)
              Unhandled Exceptions: Enable JUCE_CATCH_UNHANDLED_EXCEPTIONS to route to JUCEApplicationBase::uncaughtException(). (https://docs.juce.com/master/juce__core_8h.html)  (https://docs.juce.com/master/classjuce_1_1JUCEApplicationBase.html)