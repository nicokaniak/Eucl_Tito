https://www.youtube.com/watch?v=tgf6J8foCiw
https://juce.com/tutorials/tutorial_midi_message/
https://forum.juce.com/t/beginner-midi-sequencing/61886
https://github.com/dmultiply/Klide
https://forum.juce.com/t/midi-sequencer-where-to-beggin/57938
https://forum.juce.com/t/midi-sequencer-design-data-structures-and-synchronization/65967
https://docs.juce.com/master/classjuce_1_1MidiMessageSequence.html
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
