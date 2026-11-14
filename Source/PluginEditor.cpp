#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr int designWidth = 1200;
constexpr int designHeight = 780;
const juce::Colour ink { 0xff0a111b };
const juce::Colour panel { 0xff202d3b };
const juce::Colour cyan { 0xff70e3ed };
const juce::Colour white { 0xffe9f1f5 };
const juce::Colour muted { 0xff8c9dad };
const juce::Colour line { 0xff3b4b5c };

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

void text (juce::Graphics& g, const juce::String& value, juce::Rectangle<int> area,
           float size, juce::Colour colour = white, int justification = juce::Justification::centredLeft,
           bool bold = false)
{
    g.setColour (colour);
    g.setFont (font (size, bold));
    g.drawFittedText (value, area, justification, 1);
}

void card (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::DropShadow (juce::Colours::black.withAlpha (0.32f), 16, { 0, 6 }).drawForRectangle (g, bounds.toNearestInt());
    g.setGradientFill (juce::ColourGradient (panel.brighter (0.07f), bounds.getX(), bounds.getY(),
                                            panel.darker (0.32f), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 13.0f);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 13.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    g.drawHorizontalLine (static_cast<int> (bounds.getY()) + 1, bounds.getX() + 15.0f, bounds.getRight() - 15.0f);
}

juce::String formatDial (double value, const juce::String& id)
{
    if (id == "input" || id == "output")
        return (value > 0.05 ? "+" : "") + juce::String (value, 1) + " dB";
    if (id == "predelay" || id == "smooth")
        return juce::String (value, value < 10.0 ? 1 : 0) + " ms";
    if (id == "decay") return juce::String (value, 2) + " s";
    if (id == "rate") return juce::String (value, 2) + " Hz";
    if (id == "lowcut" || id == "highcut")
        return value >= 1000.0 ? juce::String (value / 1000.0, 1) + " kHz" : juce::String (value, 0) + " Hz";
    return juce::String (value, 0) + " %";
}
} // namespace

class GateVerbLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GateVerbLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, white);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, cyan.withAlpha (0.3f));
        setColour (juce::ComboBox::textColourId, white);
        setColour (juce::ComboBox::backgroundColourId, ink);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, muted);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17222f));
        setColour (juce::PopupMenu::textColourId, white);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff30515d));
        setColour (juce::PopupMenu::highlightedTextColourId, white);
        setColour (juce::PopupMenu::headerTextColourId, muted);
        setColour (juce::TextButton::textColourOffId, muted.brighter (0.35f));
        setColour (juce::TextButton::textColourOnId, cyan);
        setColour (juce::Label::textColourId, white);
        setColour (juce::TextEditor::backgroundColourId, ink);
        setColour (juce::TextEditor::textColourId, white);
        setColour (juce::TextEditor::outlineColourId, cyan.darker (0.4f));
        setColour (juce::TextEditor::highlightColourId, cyan.withAlpha (0.25f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff223445));
        setColour (juce::TooltipWindow::textColourId, white);
        setColour (juce::TooltipWindow::outlineColourId, line);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff17222f));
        setColour (juce::AlertWindow::textColourId, white);
        setColour (juce::AlertWindow::outlineColourId, line);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.5f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (10.7f, true); }
    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (font (12.0f));
        return label;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool over, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto active = button.getToggleState();
        auto fill = active ? juce::Colour (0xff25444e) : juce::Colour (0xff24313f);
        if (over) fill = fill.brighter (0.13f);
        if (down) fill = fill.darker (0.12f);
        const auto alpha = button.isEnabled() ? 1.0f : 0.32f;
        g.setGradientFill (juce::ColourGradient (fill.brighter (0.055f).withAlpha (alpha), 0.0f, 0.0f,
                                                fill.darker (0.12f).withAlpha (alpha), 0.0f, bounds.getHeight(), false));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour ((active ? cyan.withAlpha (0.6f) : line).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        const auto alpha = box.isEnabled() ? 1.0f : 0.35f;
        auto bounds = juce::Rectangle<float> (0.5f, 0.5f, static_cast<float> (width) - 1.0f,
                                              static_cast<float> (height) - 1.0f);
        g.setColour (ink.withAlpha (0.6f * alpha));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (line.withAlpha (alpha));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        juce::Path arrow;
        const float x = static_cast<float> (width) - 17.0f, y = static_cast<float> (height) * 0.5f;
        arrow.startNewSubPath (x - 3.5f, y - 1.5f);
        arrow.lineTo (x, y + 2.0f);
        arrow.lineTo (x + 3.5f, y - 1.5f);
        g.setColour (muted.withAlpha (alpha));
        g.strokePath (arrow, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                   static_cast<float> (width), static_cast<float> (height)).reduced (6.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
        if (radius < 3.0f) return;
        const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
        const float alpha = slider.isEnabled() ? 1.0f : 0.3f;
        auto face = juce::Rectangle<float> (cx - radius + 5.0f, cy - radius + 5.0f,
                                            radius * 2.0f - 10.0f, radius * 2.0f - 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.3f * alpha));
        g.fillEllipse (face.translated (0.0f, 3.5f).expanded (2.0f));
        juce::Path rail;
        rail.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff52616f).withAlpha (0.7f * alpha));
        g.strokePath (rail, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float angle = startAngle + position * (endAngle - startAngle);
        juce::Path value;
        value.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (cyan.withAlpha (0.12f * alpha));
        g.strokePath (value, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cyan.withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff66717c).withAlpha (alpha), face.getX(), face.getY(),
                                                juce::Colour (0xff26313e).withAlpha (alpha), face.getRight(), face.getBottom(), false));
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (0.17f * alpha));
        g.drawEllipse (face.reduced (0.5f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f * alpha));
        g.drawEllipse (face.reduced (2.0f), 1.0f);
        const float outer = radius - 10.0f, inner = outer - juce::jmax (4.0f, radius * 0.2f);
        g.setColour (white.withAlpha (alpha));
        g.drawLine (cx + std::sin (angle) * inner, cy - std::cos (angle) * inner,
                    cx + std::sin (angle) * outer, cy - std::cos (angle) * outer, 2.0f);
    }
};

class GateVerbDial final : public juce::Component
{
public:
    GateVerbDial (GateVerbAudioProcessor& p, juce::String id, juce::String title, juce::String tip)
        : parameterId (std::move (id)), name (std::move (title))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 106, 20);
        slider.setMouseDragSensitivity (175);
        slider.setVelocityBasedMode (false);
        slider.setVelocityModeParameters (0.22, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider.setScrollWheelEnabled (false);
        slider.setTooltip (tip + "  Double-click: default. Shift-drag: fine adjustment.");
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, parameterId, slider);
        slider.textFromValueFunction = [id = parameterId] (double value) { return formatDial (value, id); };
        slider.valueFromTextFunction = [id = parameterId] (const juce::String& input)
        {
            auto value = input.getDoubleValue();
            if ((id == "lowcut" || id == "highcut") && input.containsIgnoreCase ("k")) value *= 1000.0;
            return value;
        };
        if (auto* parameter = p.apvts.getParameter (parameterId))
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        slider.updateText();
    }

    void paint (juce::Graphics& g) override
    {
        text (g, name, { 0, 0, getWidth(), 16 }, 10.5f,
              isEnabled() ? muted.brighter (0.3f) : muted.withAlpha (0.35f), juce::Justification::centred, true);
    }

    void resized() override { slider.setBounds (0, 17, getWidth(), getHeight() - 17); }
    void enableControl (bool enabled) { setEnabled (enabled); slider.setEnabled (enabled); repaint(); }

private:
    juce::String parameterId, name;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class GateVerbCurveEditor final : public juce::Component
{
public:
    explicit GateVerbCurveEditor (GateVerbAudioProcessor& p) : processor (p), values (p.getCurve())
    {
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setWantsKeyboardFocus (true);
        history.fill (0.0f);
    }

    void refresh()
    {
        if (! dragging) values = processor.getCurve();
        repaint();
    }

    void resetHistory()
    {
        undoStack.clear();
        redoStack.clear();
        refresh();
    }

    void tick()
    {
        phase = processor.visualPhase.load (std::memory_order_relaxed);
        gate = processor.visualGate.load (std::memory_order_relaxed);
        const auto level = juce::jlimit (0.0f, 1.0f, processor.meterWet.load (std::memory_order_relaxed));
        const auto previous = history[(historyIndex + history.size() - 1) % history.size()];
        history[historyIndex] = juce::jmax (level, previous * 0.84f);
        historyIndex = (historyIndex + 1) % history.size();
        repaint();
    }

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    void undo()
    {
        if (undoStack.empty()) return;
        redoStack.push_back (values);
        values = undoStack.back();
        undoStack.pop_back();
        processor.setCurve (values);
        repaint();
    }

    void redo()
    {
        if (redoStack.empty()) return;
        pushUndo (values);
        values = redoStack.back();
        redoStack.pop_back();
        processor.setCurve (values);
        repaint();
    }

    void useShape (int index)
    {
        pushUndo (values);
        redoStack.clear();
        processor.applyShape (index);
        values = processor.getCurve();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff101b27));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (line.withAlpha (0.7f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
        auto plot = plotBounds();
        const bool enabled = processor.apvts.getRawParameterValue ("gate")->load() > 0.5f;
        const float activeAlpha = enabled ? 1.0f : 0.32f;

        for (int row = 0; row <= 4; ++row)
        {
            const float y = plot.getY() + plot.getHeight() * static_cast<float> (row) / 4.0f;
            g.setColour (line.withAlpha (row == 0 || row == 4 ? 0.65f : 0.34f));
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        }
        const int divisions = processor.getGridSize();
        for (int column = 0; column <= divisions; ++column)
        {
            const float x = plot.getX() + plot.getWidth() * static_cast<float> (column) / static_cast<float> (divisions);
            const bool major = column % juce::jmax (1, divisions / 4) == 0;
            g.setColour (line.withAlpha (major ? 0.65f : 0.25f));
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        }

        // This ribbon is recent measured wet-signal energy, not a synthetic waveform.
        const auto barWidth = plot.getWidth() / static_cast<float> (history.size());
        for (size_t i = 0; i < history.size(); ++i)
        {
            const auto amplitude = std::sqrt (history[(historyIndex + i) % history.size()]);
            const auto h = amplitude * plot.getHeight() * 0.62f;
            g.setColour (juce::Colour (0xff88b8d9).withAlpha (0.13f * activeAlpha));
            g.fillRoundedRectangle (plot.getX() + static_cast<float> (i) * barWidth + 1.0f,
                                    plot.getBottom() - h, barWidth - 2.0f, h, 1.0f);
        }

        juce::Path envelope;
        envelope.startNewSubPath (plot.getX(), plot.getBottom() - values[0] * plot.getHeight());
        for (size_t i = 1; i < values.size(); ++i)
            envelope.lineTo (plot.getX() + plot.getWidth() * static_cast<float> (i) / static_cast<float> (values.size() - 1),
                             plot.getBottom() - values[i] * plot.getHeight());
        auto fill = envelope;
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.22f * activeAlpha), 0.0f, plot.getY(),
                                                cyan.withAlpha (0.012f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (cyan.withAlpha (0.07f * activeAlpha));
        g.strokePath (envelope, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cyan.withAlpha (activeAlpha));
        g.strokePath (envelope, juce::PathStrokeType (2.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto x = plot.getX() + juce::jlimit (0.0f, 1.0f, phase) * plot.getWidth();
        g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.0f), x - 16.0f, 0.0f,
                                                cyan.withAlpha (0.13f * activeAlpha), x, 0.0f, false));
        g.fillRect (juce::Rectangle<float> (juce::jmax (plot.getX(), x - 16.0f), plot.getY(),
                                           juce::jmin (16.0f, x - plot.getX()), plot.getHeight()));
        g.setColour (white.withAlpha (0.8f * activeAlpha));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        const auto y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, gate) * plot.getHeight();
        g.setColour (cyan.withAlpha (0.18f * activeAlpha));
        g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        g.setColour (white.withAlpha (activeAlpha));
        g.fillEllipse (x - 3.3f, y - 3.3f, 6.6f, 6.6f);

        text (g, "100", { 3, juce::roundToInt (plot.getY()) - 6, 26, 13 }, 9.0f, muted, juce::Justification::centredRight);
        text (g, "50", { 3, juce::roundToInt (plot.getCentreY()) - 6, 26, 13 }, 9.0f, muted, juce::Justification::centredRight);
        text (g, "0", { 3, juce::roundToInt (plot.getBottom()) - 6, 26, 13 }, 9.0f, muted, juce::Justification::centredRight);
        const juce::StringArray labels { "START", "1/4", "1/2", "3/4", "END" };
        for (int i = 0; i < 5; ++i)
        {
            const int labelX = juce::roundToInt (plot.getX() + plot.getWidth() * static_cast<float> (i) * 0.25f);
            const int justification = i == 0 ? juce::Justification::centredLeft
                                            : i == 4 ? juce::Justification::centredRight : juce::Justification::centred;
            const int left = i == 0 ? labelX : i == 4 ? labelX - 48 : labelX - 24;
            text (g, labels[i], { left, getHeight() - 23, 48, 15 }, 9.0f, muted, justification);
        }
        if (! enabled)
            text (g, "GATE BYPASSED", plot.toNearestInt(), 13.0f, muted.brighter(), juce::Justification::centred, true);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        dragging = true;
        beforeDrag = values;
        lastPoint = normalise (event);
        drawBetween (lastPoint, lastPoint);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        auto point = normalise (event);
        drawBetween (lastPoint, point);
        lastPoint = point;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging && beforeDrag != values)
        {
            pushUndo (beforeDrag);
            redoStack.clear();
        }
        dragging = false;
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) && key.getKeyCode() == 'Z')
        {
            if (key.getModifiers().isShiftDown()) redo(); else undo();
            return true;
        }
        return false;
    }

private:
    juce::Rectangle<float> plotBounds() const { return getLocalBounds().toFloat().withTrimmedLeft (36.0f).withTrimmedRight (12.0f).withTrimmedTop (16.0f).withTrimmedBottom (31.0f); }

    juce::Point<float> normalise (const juce::MouseEvent& event) const
    {
        const auto plot = plotBounds();
        auto amplitude = juce::jlimit (0.0f, 1.0f, 1.0f - (event.position.y - plot.getY()) / plot.getHeight());
        if (event.mods.isShiftDown()) amplitude = std::round (amplitude * 10.0f) / 10.0f;
        return { juce::jlimit (0.0f, 1.0f, (event.position.x - plot.getX()) / plot.getWidth()), amplitude };
    }

    void drawBetween (juce::Point<float> a, juce::Point<float> b)
    {
        const auto currentValue = b.y;
        if (a.x > b.x) std::swap (a, b);
        if (processor.getEditorMode() == 1)
        {
            const int cells = processor.getGridSize();
            const int first = juce::jlimit (0, cells - 1, static_cast<int> (a.x * static_cast<float> (cells)));
            const int last = juce::jlimit (0, cells - 1, static_cast<int> (b.x * static_cast<float> (cells)));
            for (int cell = first; cell <= last; ++cell)
            {
                const auto t = last > first ? static_cast<float> (cell - first) / static_cast<float> (last - first) : 1.0f;
                const auto value = last > first ? a.y + t * (b.y - a.y) : currentValue;
                const auto begin = static_cast<size_t> (cell) * values.size() / static_cast<size_t> (cells);
                const auto end = static_cast<size_t> (cell + 1) * values.size() / static_cast<size_t> (cells);
                for (auto i = begin; i < end; ++i) values[i] = value;
            }
        }
        else
        {
            const int first = juce::roundToInt (a.x * static_cast<float> (values.size() - 1));
            const int last = juce::roundToInt (b.x * static_cast<float> (values.size() - 1));
            for (int i = first; i <= last; ++i)
            {
                const auto t = last > first ? static_cast<float> (i - first) / static_cast<float> (last - first) : 1.0f;
                values[static_cast<size_t> (i)] = last > first ? a.y + t * (b.y - a.y) : currentValue;
            }
        }
        processor.setCurve (values);
        repaint();
    }

    void pushUndo (const GateVerbAudioProcessor::Curve& state)
    {
        if (undoStack.size() >= 64) undoStack.erase (undoStack.begin());
        undoStack.push_back (state);
    }

    GateVerbAudioProcessor& processor;
    GateVerbAudioProcessor::Curve values, beforeDrag {};
    std::vector<GateVerbAudioProcessor::Curve> undoStack, redoStack;
    std::array<float, 96> history {};
    size_t historyIndex = 0;
    juce::Point<float> lastPoint;
    bool dragging = false;
    float phase = 0.0f, gate = 1.0f;
};

class GateVerbMeters final : public juce::Component
{
public:
    void setLevels (float input, float output)
    {
        in = juce::jmax (input, in * 0.88f);
        out = juce::jmax (output, out * 0.88f);
        if (input >= 1.0f) inClip = 45; else inClip = juce::jmax (0, inClip - 1);
        if (output >= 1.0f) outClip = 45; else outClip = juce::jmax (0, outClip - 1);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto half = getWidth() / 2;
        drawMeter (g, { 0, 0, half - 7, getHeight() }, in, inClip > 0);
        drawMeter (g, { half + 7, 0, half - 7, getHeight() }, out, outClip > 0);
    }

private:
    static void drawMeter (juce::Graphics& g, juce::Rectangle<int> bounds, float amplitude, bool clipped)
    {
        const auto dB = juce::Decibels::gainToDecibels (amplitude, -60.0f);
        const auto fraction = juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 60.0f);
        const auto meter = bounds.withHeight (6).toFloat();
        g.setColour (ink);
        g.fillRoundedRectangle (meter, 3.0f);
        g.setColour (clipped ? juce::Colour (0xffff9c8c) : cyan.withAlpha (0.8f));
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * fraction), 3.0f);
        text (g, amplitude < 0.001f ? "-inf dB" : juce::String (dB, 1) + " dB",
              bounds.withY (bounds.getY() + 12).withHeight (16), 10.0f, clipped ? juce::Colour (0xffff9c8c) : muted,
              juce::Justification::centred);
        if (clipped) text (g, "PEAK", bounds.withY (bounds.getY() + 30).withHeight (14), 9.0f,
                           juce::Colour (0xffff9c8c), juce::Justification::centred, true);
    }
    float in = 0.0f, out = 0.0f;
    int inClip = 0, outClip = 0;
};

GateVerbAudioProcessorEditor::GateVerbAudioProcessorEditor (GateVerbAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), look (std::make_unique<GateVerbLookAndFeel>())
{
    setLookAndFeel (look.get());
    setOpaque (true);
    addAndMakeVisible (canvas);
    curve = std::make_unique<GateVerbCurveEditor> (processor);
    meters = std::make_unique<GateVerbMeters>();
    canvas.addAndMakeVisible (*curve);
    canvas.addAndMakeVisible (*meters);

    auto dial = [this] (const juce::String& id, const juce::String& label, const juce::String& tip)
    {
        auto result = std::make_unique<GateVerbDial> (processor, id, label, tip);
        canvas.addAndMakeVisible (*result);
        return result;
    };
    inputDial = dial ("input", "IN", "Input gain before the dry and reverb paths.");
    outputDial = dial ("output", "OUT", "Final output gain after the dry/wet mix.");
    mixDial = dial ("mix", "WET / DRY", "Balance the original signal and shaped reverb. Use 100% for an aux/send return.");
    rateDial = dial ("rate", "FREE RATE", "Gate cycles per second when HOST SYNC is off.");
    depthDial = dial ("depth", "DEPTH", "Amount of amplitude shaping. At zero, the gate leaves the reverb unchanged.");
    smoothDial = dial ("smooth", "SMOOTH", "Smooth abrupt gate transitions. Low values are sharper; high values are softer.");
    phaseDial = dial ("phase", "PHASE", "Offset the gate pattern within its cycle.");
    struct ReverbControl { const char* id; const char* title; const char* tip; };
    const ReverbControl controls[] = {
        { "predelay", "PRE-DELAY", "Delay before the reverb starts." },
        { "decay", "DECAY", "Reverb decay time in seconds." },
        { "size", "SIZE", "Scale the acoustic space of the reverb." },
        { "damping", "DAMPING", "Absorb high frequencies inside the reverb tail." },
        { "diffusion", "DIFFUSION", "Control how densely early reflections blend into the tail." },
        { "lowcut", "LOW CUT", "Remove low frequencies from the reverb path." },
        { "highcut", "HIGH CUT", "Remove high frequencies from the reverb path." },
        { "width", "WIDTH", "Stereo width of the reverb; zero is mono." }
    };
    for (const auto& control : controls)
        reverbDials.add (dial (control.id, control.title, control.tip).release());

    for (auto* component : std::initializer_list<juce::Component*> {
            &presetBox, &divisionBox, &gridBox, &shapeBox, &previousPreset, &nextPreset,
            &saveButton, &loadButton, &helpButton, &freeDrawButton, &stepsButton,
            &undoButton, &redoButton, &syncButton, &gateButton, &clockLabel, &modeLabel, &versionLabel })
        canvas.addAndMakeVisible (*component);

    presetBox.addItemList (processor.getFactoryPresetNames(), 1);
    presetBox.setTooltip ("Factory presets. SAVE and LOAD store complete user presets, including your drawn gate.");
    presetBox.onChange = [this]
    {
        if (refreshing || presetBox.getSelectedId() <= 0) return;
        lastFactoryPreset = presetBox.getSelectedId() - 1;
        processor.loadFactoryPreset (lastFactoryPreset);
        curve->resetHistory();
        refreshPresetName();
    };
    previousPreset.onClick = [this] { choosePreset (-1); };
    nextPreset.onClick = [this] { choosePreset (1); };
    previousPreset.setTooltip ("Previous factory preset.");
    nextPreset.setTooltip ("Next factory preset.");
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    helpButton.onClick = [this] { showHelp(); };
    saveButton.setTooltip ("Save all parameters and the gate curve to a user preset.");
    loadButton.setTooltip ("Load a saved GATE-VERB preset.");

    divisionBox.addItemList ({ "1/64", "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "1/4", "1/4 D", "1/2", "1 bar", "2 bars", "4 bars" }, 1);
    divisionBox.setTooltip ("Length of one complete gate cycle. Bar labels use four quarter-note beats. T = triplet; D = dotted.");
    divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "division", divisionBox);
    syncButton.setClickingTogglesState (true);
    gateButton.setClickingTogglesState (true);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "sync", syncButton);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "gate", gateButton);
    syncButton.onClick = [this] { updateControlStates(); };
    gateButton.onClick = [this] { updateControlStates(); };
    syncButton.setTooltip ("On: follow the DAW musical position. Off: run continuously at FREE RATE, even when transport is stopped.");
    gateButton.setTooltip ("Enable or bypass shaping of the reverb tail. The reverb stays active.");

    for (int i : { 4, 8, 16, 32, 64 }) gridBox.addItem (juce::String (i) + " STEPS", i);
    gridBox.setSelectedId (processor.getGridSize(), juce::dontSendNotification);
    gridBox.setTooltip ("Number of divisions across one cycle. In STEPS mode, drag to set whole cells. DRAW mode stays freehand.");
    gridBox.onChange = [this]
    {
        if (! refreshing) processor.setGridSize (gridBox.getSelectedId());
        curve->repaint();
    };
    freeDrawButton.setClickingTogglesState (false);
    stepsButton.setClickingTogglesState (false);
    freeDrawButton.onClick = [this] { processor.setEditorMode (0); updateControlStates(); };
    stepsButton.onClick = [this] { processor.setEditorMode (1); updateControlStates(); };
    freeDrawButton.setTooltip ("Draw any amplitude shape with the mouse. Hold Shift to snap to 10% amplitude levels.");
    stepsButton.setTooltip ("Draw flat steps on the time grid. Hold Shift to snap amplitude to 10% levels.");
    undoButton.onClick = [this] { curve->undo(); };
    redoButton.onClick = [this] { curve->redo(); };
    undoButton.setTooltip ("Undo a gate drawing or shape change. Command-Z also works while the editor is focused.");
    redoButton.setTooltip ("Redo a gate drawing or shape change. Command-Shift-Z also works.");
    shapeBox.addItemList ({ "Open", "Pulse", "Duck", "Rise", "Fall", "Sine", "Steps" }, 1);
    shapeBox.setTextWhenNothingSelected ("QUICK SHAPES");
    shapeBox.setTooltip ("Replace the drawn curve with a starting shape. UNDO restores your previous curve.");
    shapeBox.onChange = [this]
    {
        if (shapeBox.getSelectedId() > 0)
        {
            curve->useShape (shapeBox.getSelectedId() - 1);
            shapeBox.setSelectedId (0, juce::dontSendNotification);
        }
    };

    clockLabel.setFont (font (11.0f));
    clockLabel.setColour (juce::Label::textColourId, muted);
    clockLabel.setJustificationType (juce::Justification::centredLeft);
    clockLabel.setBorderSize (juce::BorderSize<int> (0));
    modeLabel.setFont (font (17.0f));
    modeLabel.setColour (juce::Label::textColourId, cyan);
    modeLabel.setJustificationType (juce::Justification::centred);
    modeLabel.setBorderSize (juce::BorderSize<int> (0));
    versionLabel.setText ("v" JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel.setColour (juce::Label::textColourId, muted.withAlpha (0.6f));
    versionLabel.setFont (font (9.0f));
    versionLabel.setJustificationType (juce::Justification::centredRight);

    processor.addChangeListener (this);
    setResizable (true, true);
    setResizeLimits (960, 624, 1800, 1170);
    if (auto* constrainer = getConstrainer()) constrainer->setFixedAspectRatio (static_cast<double> (designWidth) / designHeight);
    setSize (designWidth, designHeight);
    refreshPresetName();
    updateControlStates();
    timerCallback();
    startTimerHz (30);
}

GateVerbAudioProcessorEditor::~GateVerbAudioProcessorEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
    fileChooser.reset();
    setLookAndFeel (nullptr);
}

void GateVerbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff142331), 0.0f, 0.0f,
                                            juce::Colour (0xff080f17), 900.0f, 780.0f, false));
    g.fillRect (0, 0, designWidth, designHeight);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff49778b).withAlpha (0.14f), 450.0f, -60.0f,
                                            juce::Colours::transparentBlack, 450.0f, 540.0f, true));
    g.fillRect (0, 0, designWidth, 590);
    g.setColour (line.withAlpha (0.3f));
    g.drawHorizontalLine (96, 24.0f, 1176.0f);

    for (int i = 0; i < 4; ++i)
    {
        g.setColour (cyan.withAlpha (0.4f + static_cast<float> (i) * 0.18f));
        g.fillRoundedRectangle (28.0f + static_cast<float> (i) * 8.0f, 37.0f + static_cast<float> (3 - i) * 5.0f,
                                4.0f, 25.0f - static_cast<float> (3 - i) * 5.0f, 2.0f);
    }
    text (g, "GATE-VERB", { 75, 23, 400, 43 }, 35.0f, white, juce::Justification::centredLeft, true);
    text (g, "by studiobro.net for Sound&Recording Magazin", { 77, 67, 480, 17 }, 12.0f, muted);
    text (g, "PRESET", { 612, 19, 250, 15 }, 9.5f, muted, juce::Justification::centredLeft, true);

    card (g, { 24.0f, 112.0f, 904.0f, 462.0f });
    card (g, { 948.0f, 112.0f, 228.0f, 462.0f });
    card (g, { 24.0f, 594.0f, 1152.0f, 162.0f });
    text (g, "TAIL SHAPER", { 44, 130, 280, 22 }, 14.0f, white, juce::Justification::centredLeft, true);
    text (g, "Sculpt the amplitude of the reverb tail", { 44, 153, 400, 17 }, 11.0f, muted);
    text (g, "Drag to draw   /   Shift: snap amplitude   /   The bright cursor follows the audio clock",
          { 80, 429, 720, 15 }, 9.5f, muted.withAlpha (0.85f));
    text (g, "CLOCK", { 44, 451, 230, 14 }, 9.5f, muted, juce::Justification::centredLeft, true);
    text (g, "GATE", { 836, 451, 58, 14 }, 9.5f, muted, juce::Justification::centred, true);
    text (g, "GAIN", { 836, 510, 58, 14 }, 9.0f, muted, juce::Justification::centred);
    g.setColour (line.withAlpha (0.5f));
    g.drawVerticalLine (286, 465.0f, 548.0f);

    text (g, "SIGNAL", { 968, 130, 184, 22 }, 14.0f, white, juce::Justification::centredLeft, true);
    text (g, "INPUT / OUTPUT", { 968, 153, 184, 17 }, 10.0f, muted);
    g.setColour (line.withAlpha (0.45f));
    g.drawHorizontalLine (349, 968.0f, 1156.0f);
    text (g, "DRY", { 976, 529, 60, 17 }, 9.5f, muted);
    text (g, "WET", { 1092, 529, 60, 17 }, 9.5f, muted, juce::Justification::centredRight);
    text (g, "100% WET FOR SENDS", { 968, 550, 188, 13 }, 9.0f, muted.withAlpha (0.7f), juce::Justification::centred);

    text (g, "REVERB", { 44, 607, 150, 22 }, 13.0f, white, juce::Justification::centredLeft, true);
    text (g, "Build the space. Then give it a rhythm.", { 179, 611, 450, 16 }, 10.5f, muted);
    text (g, "studiobro.net", { 26, 762, 300, 15 }, 9.0f, muted.withAlpha (0.65f));
    text (g, "IN  /  REVERB  /  GATE  /  WET + DRY  /  OUT", { 370, 762, 470, 15 }, 9.0f,
          muted.withAlpha (0.55f), juce::Justification::centred);
}

void GateVerbAudioProcessorEditor::resized()
{
    scale = static_cast<float> (getWidth()) / static_cast<float> (designWidth);
    canvas.setBounds (0, 0, designWidth, designHeight);
    canvas.setTransform (juce::AffineTransform::scale (scale));
    previousPreset.setBounds (574, 42, 28, 32);
    presetBox.setBounds (612, 42, 265, 32);
    nextPreset.setBounds (887, 42, 28, 32);
    saveButton.setBounds (937, 42, 75, 32);
    loadButton.setBounds (1020, 42, 75, 32);
    helpButton.setBounds (1136, 42, 36, 32);
    gateButton.setBounds (810, 132, 98, 30);
    freeDrawButton.setBounds (44, 180, 75, 28);
    stepsButton.setBounds (125, 180, 75, 28);
    gridBox.setBounds (218, 180, 112, 28);
    undoButton.setBounds (348, 180, 70, 28);
    redoButton.setBounds (424, 180, 70, 28);
    shapeBox.setBounds (696, 180, 212, 28);
    curve->setBounds (44, 218, 864, 208);
    syncButton.setBounds (44, 473, 112, 30);
    divisionBox.setBounds (167, 473, 104, 30);
    clockLabel.setBounds (44, 512, 230, 25);
    rateDial->setBounds (302, 452, 116, 105);
    depthDial->setBounds (435, 452, 116, 105);
    smoothDial->setBounds (568, 452, 116, 105);
    phaseDial->setBounds (701, 452, 116, 105);
    modeLabel.setBounds (831, 473, 68, 31);
    inputDial->setBounds (962, 185, 94, 106);
    outputDial->setBounds (1068, 185, 94, 106);
    meters->setBounds (968, 303, 188, 46);
    mixDial->setBounds (991, 369, 142, 149);
    for (int i = 0; i < reverbDials.size(); ++i)
        reverbDials[i]->setBounds (39 + i * 140, 637, 142, 110);
    versionLabel.setBounds (1080, 761, 91, 17);
}

void GateVerbAudioProcessorEditor::timerCallback()
{
    curve->tick();
    meters->setLevels (processor.meterIn.load (std::memory_order_relaxed), processor.meterOut.load (std::memory_order_relaxed));
    const bool sync = processor.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    const bool playing = processor.visualPlaying.load (std::memory_order_relaxed);
    const bool hasHost = processor.visualHasHost.load (std::memory_order_relaxed);
    const auto bpm = processor.visualBpm.load (std::memory_order_relaxed);
    juce::String status;
    if (! sync) status = "FREE RUN  /  CONTINUOUS";
    else if (! hasHost) status = "120 BPM  /  INTERNAL FALLBACK";
    else status = juce::String (bpm, 1) + " BPM  /  " + (playing ? "PLAYING" : "STOPPED");
    clockLabel.setText (status, juce::dontSendNotification);
    modeLabel.setText (juce::String (juce::roundToInt (processor.visualGate.load (std::memory_order_relaxed) * 100.0f)) + "%", juce::dontSendNotification);
    undoButton.setEnabled (curve->canUndo());
    redoButton.setEnabled (curve->canRedo());
    updateControlStates();
    refreshPresetName();
}

void GateVerbAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshing = true;
    gridBox.setSelectedId (processor.getGridSize(), juce::dontSendNotification);
    refreshing = false;
    curve->refresh();
    updateControlStates();
    refreshPresetName();
}

void GateVerbAudioProcessorEditor::updateControlStates()
{
    const bool sync = processor.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    const bool gateOn = processor.apvts.getRawParameterValue ("gate")->load() > 0.5f;
    divisionBox.setEnabled (sync);
    if (rateDial->isEnabled() == sync) rateDial->enableControl (! sync);
    syncButton.setButtonText (sync ? "HOST SYNC" : "FREE RUN");
    gateButton.setButtonText (gateOn ? "GATE ON" : "GATE OFF");
    freeDrawButton.setToggleState (processor.getEditorMode() == 0, juce::dontSendNotification);
    stepsButton.setToggleState (processor.getEditorMode() == 1, juce::dontSendNotification);
}

void GateVerbAudioProcessorEditor::refreshPresetName()
{
    const auto title = processor.getPresetName() + (processor.isPresetDirty() ? "  *" : "");
    if (presetBox.getText() != title)
    {
        refreshing = true;
        presetBox.setText (title, juce::dontSendNotification);
        refreshing = false;
    }
}

void GateVerbAudioProcessorEditor::choosePreset (int direction)
{
    const auto count = processor.getFactoryPresetNames().size();
    if (count <= 0) return;
    lastFactoryPreset = (lastFactoryPreset + direction + count) % count;
    processor.loadFactoryPreset (lastFactoryPreset);
    curve->resetHistory();
    refreshPresetName();
}

void GateVerbAudioProcessorEditor::savePreset()
{
    auto directory = GateVerbAudioProcessor::getUserPresetDirectory();
    directory.createDirectory();
    auto name = juce::File::createLegalFileName (processor.getPresetName());
    if (name.isEmpty()) name = "My Gate";
    fileChooser = std::make_unique<juce::FileChooser> ("Save GATE-VERB preset", directory.getChildFile (name + ".gateverb"), "*.gateverb");
    juce::Component::SafePointer<GateVerbAudioProcessorEditor> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
                             [safe] (const juce::FileChooser& chooser)
    {
        if (safe == nullptr || chooser.getResult() == juce::File()) return;
        safe->showPresetError (safe->processor.saveUserPreset (chooser.getResult().withFileExtension ("gateverb")));
        safe->refreshPresetName();
    });
}

void GateVerbAudioProcessorEditor::loadPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load GATE-VERB preset", GateVerbAudioProcessor::getUserPresetDirectory(), "*.gateverb");
    juce::Component::SafePointer<GateVerbAudioProcessorEditor> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [safe] (const juce::FileChooser& chooser)
    {
        if (safe == nullptr || chooser.getResult() == juce::File()) return;
        auto result = safe->processor.loadUserPreset (chooser.getResult());
        safe->showPresetError (result);
        if (result.wasOk()) safe->curve->resetHistory();
        safe->refreshPresetName();
    });
}

void GateVerbAudioProcessorEditor::showPresetError (const juce::Result& result)
{
    if (result.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                               "GATE-VERB preset", result.getErrorMessage(), "OK", this);
}

void GateVerbAudioProcessorEditor::showHelp()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "GATE-VERB / Quick guide",
        "Draw the volume shape of the reverb tail in the large editor. The original dry signal is unaffected by the gate.\n\n"
        "DRAW: freehand curves. STEPS: paint whole cells on the selected grid. Hold Shift to snap amplitude to 10% levels. "
        "QUICK SHAPES supplies starting shapes; UNDO and REDO operate on curve edits.\n\n"
        "HOST SYNC follows the DAW musical position; select the length of a complete cycle beside it. Bar labels use four quarter-note beats. "
        "Switch to FREE RUN to use the frequency knob and keep the pattern running while transport is stopped.\n\n"
        "DEPTH blends shaping in or out. SMOOTH rounds sharp edges. PHASE shifts the pattern. GATE OFF leaves the reverb active.\n\n"
        "Use WET/DRY at 100% on an aux/send. Double-click a knob for its default value, Shift-drag for fine changes, "
        "or click its value to enter a number. SAVE and LOAD include every parameter and the gate curve. "
        "The DAW also saves the complete curve and settings with your project.", "GOT IT", this);
}
