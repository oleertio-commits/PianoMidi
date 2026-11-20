#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

// PianoMidi: convierte audio polifonico (piano) en notas MIDI.
class PianoMidiProcessor : public juce::AudioProcessor,
                           private juce::Timer
{
public:
    static constexpr int fftOrder = 14;
    static constexpr int fftSize  = 1 << fftOrder;   // 16384 muestras
    static constexpr int hop      = 2048;            // analisis cada 2048 muestras
    static constexpr int onFrames = 2;               // cuadros para confirmar nota
    static constexpr int offFrames = 3;              // cuadros para soltar nota

    PianoMidiProcessor();
    ~PianoMidiProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PianoMidi"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- usado por el editor ---
    bool  isNoteActive (int note) const noexcept;
    float getLevel() const noexcept { return level.load(); }
    int   getRecordedCount() const;
    void  clearRecording();
    bool  exportMidiFile (const juce::File& file);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct NoteEvent { int64_t sample; int note; int velocity; bool on; };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void analyse (int sampleOffset, int64_t absoluteSample, juce::MidiBuffer& midi, int channel);
    void emit (juce::MidiBuffer& midi, int channel, int sampleOffset, int64_t absSample,
               int note, int velocity, bool on);
    void updateMasks();
    void timerCallback() override;

    juce::dsp::FFT fft;
    std::vector<float> ring, fftData, mag, window;
    std::array<float, 128> noteFreq {};
    int writePos = 0, hopCount = 0;
    double sampleRate = 44100.0;
    int64_t samplesProcessed = 0;

    std::array<int, 128>  onCount {}, offCount {}, lastVel {};
    std::array<bool, 128> sounding {};
    std::atomic<bool> needAllOff { false };

    std::atomic<uint64_t> maskLo { 0 }, maskHi { 0 };
    std::atomic<float> level { 0.0f };

    std::atomic<float> *pThreshold = nullptr, *pVoices = nullptr, *pLow = nullptr,
                       *pHigh = nullptr, *pChannel = nullptr, *pThru = nullptr;

    // cola sin bloqueos audio -> hilo de mensajes
    juce::AbstractFifo fifo { 4096 };
    std::array<NoteEvent, 4096> fifoBuf {};
    mutable juce::CriticalSection recLock;
    std::vector<NoteEvent> recorded;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoMidiProcessor)
};
