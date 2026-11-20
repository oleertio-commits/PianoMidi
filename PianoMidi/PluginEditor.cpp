#include "PluginEditor.h"

using namespace juce;

namespace
{
    const Colour kGold  { 0xffffc857 };
    const Colour kCyan  { 0xff4de1ff };
    const Colour kPanel { 0xff1a1d3a };
}

//==============================================================================
PianoLookAndFeel::PianoLookAndFeel()
{
    setColour (Label::textColourId, Colours::white.withAlpha (0.85f));
    setColour (Slider::textBoxTextColourId, kGold);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId, kPanel);
    setColour (ComboBox::textColourId, Colours::white);
    setColour (ComboBox::outlineColourId, kCyan.withAlpha (0.5f));
    setColour (ComboBox::arrowColourId, kGold);
    setColour (PopupMenu::backgroundColourId, kPanel);
    setColour (PopupMenu::textColourId, Colours::white);
    setColour (PopupMenu::highlightedBackgroundColourId, kGold.withAlpha (0.35f));
    setColour (TextButton::textColourOffId, Colours::white);
    setColour (TextButton::textColourOnId, Colours::white);
}

void PianoLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startA, float endA, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float arcR = r - 3.0f;
    const float angle = startA + pos * (endA - startA);

    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, endA, true);
    g.setColour (Colour (0xff33385e));
    g.strokePath (track, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, angle, true);
    g.setColour (kGold.withAlpha (0.25f));
    g.strokePath (value, PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (kGold);
    g.strokePath (value, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const float br = r - 11.0f;
    g.setGradientFill (ColourGradient (Colour (0xff343a6b), c.x, c.y - br, Colour (0xff0e1126), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (kCyan.withAlpha (0.35f));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.2f);

    const Point<float> p1 (c.x + std::sin (angle) * br * 0.35f, c.y - std::cos (angle) * br * 0.35f);
    const Point<float> p2 (c.x + std::sin (angle) * br * 0.92f, c.y - std::cos (angle) * br * 0.92f);
    g.setColour (kGold);
    g.drawLine (Line<float> (p1, p2), 3.0f);
}

void PianoLookAndFeel::drawButtonBackground (Graphics& g, Button& btn, const Colour&, bool hl, bool down)
{
    auto b = btn.getLocalBounds().toFloat().reduced (1.0f);
    auto base = down ? kGold.withAlpha (0.45f) : (hl ? kPanel.brighter (0.25f) : kPanel);
    g.setColour (base);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (kGold.withAlpha (hl ? 0.9f : 0.5f));
    g.drawRoundedRectangle (b, 8.0f, 1.3f);
}

void PianoLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& t, bool hl, bool)
{
    auto b = t.getLocalBounds().toFloat();
    auto box = b.removeFromLeft (b.getHeight()).reduced (5.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (hl ? kGold : kCyan.withAlpha (0.6f));
    g.drawRoundedRectangle (box, 5.0f, 1.4f);
    if (t.getToggleState())
    {
        g.setColour (kGold);
        g.fillRoundedRectangle (box.reduced (5.0f), 3.0f);
    }
    g.setColour (Colours::white.withAlpha (0.9f));
    g.setFont (Font (FontOptions (14.0f)));
    g.drawText (t.getButtonText(), b.withTrimmedLeft (6.0f), Justification::centredLeft);
}

//==============================================================================
class PianoMidiEditor::DragPad : public Component
{
public:
    explicit DragPad (PianoMidiProcessor& p) : proc (p)
    {
        setMouseCursor (MouseCursor::DraggingHandCursor);
    }

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kCyan.withAlpha (0.12f));
        g.fillRoundedRectangle (b, 8.0f);
        const float dash[] = { 5.0f, 4.0f };
        Path outline; outline.addRoundedRectangle (b, 8.0f);
        Path dashed;
        PathStrokeType (1.4f).createDashedStroke (dashed, outline, dash, 2);
        g.setColour (kCyan);
        g.fillPath (dashed);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.setFont (Font (FontOptions (14.0f)));
        g.drawText ("Arrastra aqui el MIDI a una pista", getLocalBounds(), Justification::centred);
    }

    void mouseDown (const MouseEvent&) override { dragging = false; }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("PianoMidi_grabacion.mid");
        if (proc.exportMidiFile (f))
            DragAndDropContainer::performExternalDragDropOfFiles (StringArray { f.getFullPathName() }, false, this);
    }

private:
    PianoMidiProcessor& proc;
    bool dragging = false;
};

//==============================================================================
PianoMidiEditor::PianoMidiEditor (PianoMidiProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (thresholdKnob, "threshold", "Umbral");
    setupKnob (voicesKnob,    "voices",    "Voces");
    setupKnob (lowKnob,       "lowNote",   "Nota grave");
    setupKnob (highKnob,      "highNote",  "Nota aguda");

    channelLabel.setText ("Canal MIDI", dontSendNotification);
    channelLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (channelLabel);

    if (auto* param = dynamic_cast<AudioParameterChoice*> (processor.apvts.getParameter ("channel")))
        channelBox.addItemList (param->choices, 1);
    addAndMakeVisible (channelBox);
    channelAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "channel", channelBox);

    addAndMakeVisible (thruButton);
    thruAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "thru", thruButton);

    addAndMakeVisible (saveButton);
    addAndMakeVisible (clearButton);
    saveButton.onClick = [this]
    {
        auto& proc = processor;
        chooser = std::make_unique<FileChooser> ("Guardar MIDI",
                    File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("PianoMidi.mid"), "*.mid");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                                | FileBrowserComponent::warnAboutOverwriting,
                              [&proc] (const FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f != File()) proc.exportMidiFile (f.withFileExtension ("mid"));
                              });
    };
    clearButton.onClick = [this] { processor.clearRecording(); };

    dragPad = std::make_unique<DragPad> (processor);
    addAndMakeVisible (*dragPad);

    statusLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (statusLabel);

    setSize (760, 440);
    startTimerHz (30);
}

PianoMidiEditor::~PianoMidiEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PianoMidiEditor::setupKnob (Knob& k, const String& paramId, const String& text)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 20);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    k.label.setText (text, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (Font (FontOptions (15.0f, Font::bold)));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k.slider);
}

void PianoMidiEditor::timerCallback()
{
    statusLabel.setText ("Eventos grabados: " + String (processor.getRecordedCount()), dontSendNotification);
    repaint();
}

void PianoMidiEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    r.removeFromTop (56);

    auto knobRow = r.removeFromTop (150);
    auto right = knobRow.removeFromRight (250);
    const int kw = knobRow.getWidth() / 4;
    for (auto* k : { &thresholdKnob, &voicesKnob, &lowKnob, &highKnob })
    {
        auto a = knobRow.removeFromLeft (kw);
        k->label.setBounds (a.removeFromTop (22));
        k->slider.setBounds (a);
    }

    right.removeFromLeft (30);
    right.removeFromTop (20);
    channelLabel.setBounds (right.removeFromTop (22));
    channelBox.setBounds (right.removeFromTop (30));
    right.removeFromTop (14);
    thruButton.setBounds (right.removeFromTop (30));

    r.removeFromTop (10);
    auto btnRow = r.removeFromTop (34);
    saveButton.setBounds (btnRow.removeFromLeft (130));
    btnRow.removeFromLeft (10);
    clearButton.setBounds (btnRow.removeFromLeft (90));
    btnRow.removeFromLeft (10);
    dragPad->setBounds (btnRow);

    r.removeFromTop (10);
    auto meterRow = r.removeFromTop (18);
    meterArea = meterRow.removeFromLeft (320);
    statusLabel.setBounds (meterRow);

    r.removeFromTop (8);
    pianoArea = r;
}

void PianoMidiEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Colour (0xff0b0d1f), 0.0f, 0.0f,
                                       Colour (0xff2a1650), 0.0f, (float) getHeight(), false));
    g.fillAll();

    // halo dorado de fondo
    g.setGradientFill (ColourGradient (kGold.withAlpha (0.10f), (float) getWidth() * 0.15f, 0.0f,
                                       Colours::transparentBlack, (float) getWidth() * 0.6f, 160.0f, true));
    g.fillRect (0, 0, getWidth(), 200);

    g.setColour (kGold);
    g.setFont (Font (FontOptions (34.0f, Font::bold)));
    g.drawText ("PianoMidi", 24, 12, 300, 40, Justification::centredLeft);
    g.setColour (kCyan.withAlpha (0.85f));
    g.setFont (Font (FontOptions (13.0f)));
    g.drawText ("AUDIO A MIDI POLIFONICO", 26, 46, 300, 16, Justification::centredLeft);

    // medidor de entrada
    auto m = meterArea.toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (m, 5.0f);
    const float lvl = jlimit (0.0f, 1.0f, processor.getLevel());
    const float shown = std::sqrt (lvl);
    g.setGradientFill (ColourGradient (kCyan, m.getX(), 0.0f, kGold, m.getRight(), 0.0f, false));
    g.fillRoundedRectangle (m.withWidth (jmax (4.0f, m.getWidth() * shown)).reduced (1.0f), 4.0f);
    g.setColour (Colours::white.withAlpha (0.7f));
    g.setFont (Font (FontOptions (11.0f)));
    g.drawText ("ENTRADA", meterArea.reduced (8, 0), Justification::centredRight);

    drawPiano (g, pianoArea);
}

void PianoMidiEditor::drawPiano (Graphics& g, Rectangle<int> area)
{
    constexpr int first = 21, last = 108;
    auto isBlack = [] (int n) { const int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; };

    int whites = 0;
    for (int n = first; n <= last; ++n) if (! isBlack (n)) ++whites;

    const float ww = (float) area.getWidth() / (float) whites;
    const float bw = ww * 0.62f;
    const float bh = (float) area.getHeight() * 0.62f;
    const int lo = jmin ((int) *processor.apvts.getRawParameterValue ("lowNote"),
                         (int) *processor.apvts.getRawParameterValue ("highNote"));
    const int hi = jmax ((int) *processor.apvts.getRawParameterValue ("lowNote"),
                         (int) *processor.apvts.getRawParameterValue ("highNote"));

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (area.toFloat().expanded (2.0f), 4.0f);

    int wi = 0;
    for (int n = first; n <= last; ++n)
    {
        if (isBlack (n)) continue;
        Rectangle<float> k ((float) area.getX() + (float) wi * ww, (float) area.getY(), ww - 1.0f, (float) area.getHeight());
        const bool on = processor.isNoteActive (n);
        g.setColour (on ? kGold : Colour (0xfff2f0ea));
        g.fillRoundedRectangle (k, 2.0f);
        if (n < lo || n > hi)
        {
            g.setColour (Colour (0xff0b0d1f).withAlpha (0.55f));
            g.fillRoundedRectangle (k, 2.0f);
        }
        ++wi;
    }

    wi = 0;
    for (int n = first; n <= last; ++n)
    {
        if (! isBlack (n)) { ++wi; continue; }
        Rectangle<float> k ((float) area.getX() + (float) wi * ww - bw * 0.5f, (float) area.getY(), bw, bh);
        const bool on = processor.isNoteActive (n);
        g.setColour (on ? kCyan : Colour (0xff12142b));
        g.fillRoundedRectangle (k, 2.0f);
        if (on)
        {
            g.setColour (kCyan.withAlpha (0.35f));
            g.drawRoundedRectangle (k.expanded (1.5f), 3.0f, 2.0f);
        }
        else if (n < lo || n > hi)
        {
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (k, 2.0f);
        }
    }
}
