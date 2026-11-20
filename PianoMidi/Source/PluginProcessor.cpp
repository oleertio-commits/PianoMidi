#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace juce;

namespace
{
    constexpr int kMaxEvents = 200000;
    constexpr int kHarmonics = 6;
    constexpr float kWeights[kHarmonics] = { 1.0f, 0.8f, 0.65f, 0.5f, 0.4f, 0.3f };
}

AudioProcessorValueTreeState::ParameterLayout PianoMidiProcessor::createLayout()
{
    auto noteText = [] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 4); };
    auto noteParse = [] (const String& t)
    {
        for (int n = 0; n < 128; ++n)
            if (MidiMessage::getMidiNoteName (n, true, true, 4).equalsIgnoreCase (t.trim()))
                return n;
        return t.getIntValue();
    };

    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "threshold", 1 }, "Umbral",
        NormalisableRange<float> (-60.0f, -15.0f, 0.1f), -38.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "voices", 1 }, "Voces", 1, 10, 6));

    p.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "lowNote", 1 }, "Nota grave", 21, 108, 36,
        AudioParameterIntAttributes().withStringFromValueFunction (noteText)
                                     .withValueFromStringFunction (noteParse)));

    p.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "highNote", 1 }, "Nota aguda", 21, 108, 96,
        AudioParameterIntAttributes().withStringFromValueFunction (noteText)
                                     .withValueFromStringFunction (noteParse)));

    StringArray channels;
    for (int i = 1; i <= 16; ++i) channels.add (String (i));
    p.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "channel", 1 }, "Canal MIDI", channels, 0));

    p.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "thru", 1 }, "Audio directo", true));

    return { p.begin(), p.end() };
}

PianoMidiProcessor::PianoMidiProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PianoMidi", createLayout()),
      fft (fftOrder),
      ring (fftSize, 0.0f),
      fftData (2 * fftSize, 0.0f),
      mag (fftSize / 2 + 1, 0.0f),
      window (fftSize, 0.0f)
{
    for (int i = 0; i < fftSize; ++i)   // ventana Hann
        window[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));

    for (int n = 0; n < 128; ++n)
        noteFreq[(size_t) n] = 440.0f * std::pow (2.0f, ((float) n - 69.0f) / 12.0f);

    pThreshold = apvts.getRawParameterValue ("threshold");
    pVoices    = apvts.getRawParameterValue ("voices");
    pLow       = apvts.getRawParameterValue ("lowNote");
    pHigh      = apvts.getRawParameterValue ("highNote");
    pChannel   = apvts.getRawParameterValue ("channel");
    pThru      = apvts.getRawParameterValue ("thru");

    recorded.reserve (4096);
    startTimerHz (20);
}

PianoMidiProcessor::~PianoMidiProcessor() { stopTimer(); }

void PianoMidiProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = hopCount = 0;
    onCount.fill (0);
    offCount.fill (0);
    bool any = false;
    for (bool s : sounding) any = any || s;
    if (any) needAllOff = true;       // soltar notas colgadas en el siguiente bloque
    level = 0.0f;
}

bool PianoMidiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void PianoMidiProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    midi.clear();

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int channel = jlimit (1, 16, (int) pChannel->load() + 1);

    if (needAllOff.exchange (false))
    {
        for (int n = 0; n < 128; ++n)
            if (sounding[(size_t) n]) { midi.addEvent (MidiMessage::noteOff (channel, n), 0); sounding[(size_t) n] = false; }
        updateMasks();
    }

    int64_t blockStart = samplesProcessed;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto t = pos->getTimeInSamples())
                blockStart = *t;

    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < numChannels; ++c) s += buffer.getReadPointer (c)[i];
        if (numChannels > 0) s /= (float) numChannels;

        peak = jmax (peak, std::abs (s));
        ring[(size_t) writePos] = s;
        writePos = (writePos + 1) & (fftSize - 1);

        if (++hopCount >= hop)
        {
            hopCount = 0;
            analyse (i, blockStart + i, midi, channel);
        }
    }

    level.store (jmax (peak, level.load() * 0.85f));
    samplesProcessed += numSamples;

    if (pThru->load() < 0.5f)
        buffer.clear();
}

void PianoMidiProcessor::analyse (int sampleOffset, int64_t absSample, MidiBuffer& midi, int channel)
{
    // 1) FFT de la ventana mas reciente
    for (int k = 0; k < fftSize; ++k)
        fftData[(size_t) k] = ring[(size_t) ((writePos + k) & (fftSize - 1))] * window[(size_t) k];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float norm = 4.0f / (float) fftSize;   // amplitud ~ pico de la senoidal
    const int nBins = fftSize / 2;
    for (int k = 0; k <= nBins; ++k) mag[(size_t) k] = fftData[(size_t) k] * norm;

    // 2) seleccion iterativa de notas (suma armonica + supresion)
    const float thr   = Decibels::decibelsToGain (pThreshold->load());
    const int   lo    = jmin ((int) pLow->load(), (int) pHigh->load());
    const int   hi    = jmax ((int) pLow->load(), (int) pHigh->load());
    const int   voices = jlimit (1, 10, (int) pVoices->load());
    const float binHz = (float) sampleRate / (float) fftSize;

    std::array<bool, 128> detected {};
    std::array<int, 128>  velocity {};
    float firstS = 0.0f;

    for (int v = 0; v < voices; ++v)
    {
        int best = -1;
        float bestS = 0.0f;

        for (int n = lo; n <= hi; ++n)
        {
            if (detected[(size_t) n]) continue;
            float S = 0.0f, a1 = 0.0f;
            for (int h = 1; h <= kHarmonics; ++h)
            {
                const float fh = noteFreq[(size_t) n] * (float) h;
                if (fh >= (float) sampleRate * 0.45f) break;
                const int c = (int) (fh / binHz + 0.5f);
                if (c + 1 >= nBins) break;
                const float a = jmax (mag[(size_t) c - 1], mag[(size_t) c], mag[(size_t) c + 1]);
                if (h == 1) a1 = a;
                S += kWeights[h - 1] * a;
            }
            if (a1 < 0.5f * thr) continue;      // exige fundamental presente
            if (S > bestS) { bestS = S; best = n; }
        }

        if (best < 0 || bestS < thr) break;
        if (v == 0) firstS = bestS;
        else if (bestS < 0.15f * firstS) break;

        detected[(size_t) best] = true;
        const float db = Decibels::gainToDecibels (bestS / thr);
        velocity[(size_t) best] = jlimit (25, 127, (int) (35.0f + db * 2.5f));

        for (int h = 1; h <= 8; ++h)            // atenuar armonicos de la nota elegida
        {
            const float fh = noteFreq[(size_t) best] * (float) h;
            const int c = (int) (fh / binHz + 0.5f);
            if (c + 1 >= nBins) break;
            for (int k = c - 1; k <= c + 1; ++k) mag[(size_t) k] *= 0.4f;
        }
    }

    // 3) histeresis temporal y emision de eventos
    for (int n = 0; n < 128; ++n)
    {
        const auto i = (size_t) n;
        if (detected[i])
        {
            offCount[i] = 0;
            lastVel[i] = velocity[i];
            if (++onCount[i] >= onFrames && ! sounding[i])
            {
                sounding[i] = true;
                emit (midi, channel, sampleOffset, absSample, n, lastVel[i], true);
            }
        }
        else
        {
            onCount[i] = 0;
            if (sounding[i] && ++offCount[i] >= offFrames)
            {
                sounding[i] = false;
                emit (midi, channel, sampleOffset, absSample, n, 0, false);
            }
        }
    }
    updateMasks();
}

void PianoMidiProcessor::emit (MidiBuffer& midi, int channel, int sampleOffset, int64_t absSample,
                               int note, int velocity, bool on)
{
    if (on) midi.addEvent (MidiMessage::noteOn  (channel, note, (uint8) velocity), sampleOffset);
    else    midi.addEvent (MidiMessage::noteOff (channel, note), sampleOffset);

    // compensar el retardo del analisis para el MIDI exportado
    const int64_t comp = fftSize / 2 + (int64_t) hop * ((on ? onFrames : offFrames) - 1);
    NoteEvent e { jmax<int64_t> (0, absSample - comp), note, velocity, on };

    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { fifoBuf[(size_t) s1] = e; fifo.finishedWrite (1); }
}

void PianoMidiProcessor::updateMasks()
{
    uint64_t lo = 0, hi = 0;
    for (int n = 0; n < 64; ++n)   if (sounding[(size_t) n])        lo |= (uint64_t) 1 << n;
    for (int n = 64; n < 128; ++n) if (sounding[(size_t) n]) hi |= (uint64_t) 1 << (n - 64);
    maskLo.store (lo);
    maskHi.store (hi);
}

bool PianoMidiProcessor::isNoteActive (int n) const noexcept
{
    if (n < 0 || n > 127) return false;
    return n < 64 ? ((maskLo.load() >> n) & 1u) != 0 : ((maskHi.load() >> (n - 64)) & 1u) != 0;
}

void PianoMidiProcessor::timerCallback()
{
    const int ready = fifo.getNumReady();
    if (ready <= 0) return;

    int s1, n1, s2, n2;
    fifo.prepareToRead (ready, s1, n1, s2, n2);
    {
        const ScopedLock sl (recLock);
        for (int i = 0; i < n1; ++i) if ((int) recorded.size() < kMaxEvents) recorded.push_back (fifoBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) if ((int) recorded.size() < kMaxEvents) recorded.push_back (fifoBuf[(size_t) (s2 + i)]);
    }
    fifo.finishedRead (n1 + n2);
}

int PianoMidiProcessor::getRecordedCount() const
{
    const ScopedLock sl (recLock);
    return (int) recorded.size();
}

void PianoMidiProcessor::clearRecording()
{
    const ScopedLock sl (recLock);
    recorded.clear();
}

bool PianoMidiProcessor::exportMidiFile (const File& file)
{
    std::vector<NoteEvent> events;
    {
        const ScopedLock sl (recLock);
        events = recorded;
    }
    if (events.empty()) return false;

    std::stable_sort (events.begin(), events.end(),
                      [] (const NoteEvent& a, const NoteEvent& b) { return a.sample < b.sample; });

    constexpr int ppq = 960;                       // a 120 BPM: 1 s = 1920 ticks
    const double ticksPerSample = 2.0 * ppq / sampleRate;
    const int channel = jlimit (1, 16, (int) pChannel->load() + 1);

    MidiMessageSequence notes, tempo;
    tempo.addEvent (MidiMessage::tempoMetaEvent (500000), 0.0);
    for (auto& e : events)
        notes.addEvent (e.on ? MidiMessage::noteOn (channel, e.note, (uint8) e.velocity)
                             : MidiMessage::noteOff (channel, e.note),
                        (double) e.sample * ticksPerSample);
    notes.updateMatchedPairs();

    MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (tempo);
    mf.addTrack (notes);

    file.deleteFile();
    FileOutputStream out (file);
    if (! out.openedOk()) return false;
    return mf.writeTo (out);
}

void PianoMidiProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PianoMidiProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* PianoMidiProcessor::createEditor() { return new PianoMidiEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PianoMidiProcessor(); }
