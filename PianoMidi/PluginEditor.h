#pragma once
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Estetica "piano de medianoche": fondo azul noche/violeta, acentos dorados y cian.
class PianoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PianoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class PianoMidiEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit PianoMidiEditor (PianoMidiProcessor&);
    ~PianoMidiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class DragPad;
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void drawPiano (juce::Graphics&, juce::Rectangle<int>);
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& text);

    PianoMidiProcessor& processor;
    PianoLookAndFeel lnf;

    Knob thresholdKnob, voicesKnob, lowKnob, highKnob;
    juce::ComboBox channelBox;
    juce::Label channelLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;
    juce::ToggleButton thruButton { "Audio directo" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> thruAttachment;
    juce::TextButton saveButton { "Guardar .mid" }, clearButton { "Limpiar" };
    std::unique_ptr<DragPad> dragPad;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Label statusLabel;
    juce::Rectangle<int> pianoArea, meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoMidiEditor)
};
