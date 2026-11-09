#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstdint>

namespace
{
    // pannello del pedale: alluminio verniciato bianco avorio su scatola di plastica nera
    const juce::Colour plateLight (0xfff3f1ea);
    const juce::Colour plateMid   (0xffe7e4da);
    const juce::Colour plateDark  (0xffd2cec2);
    const juce::Colour ink        (0xff1b1a19);   // serigrafia
    const juce::Colour cream      (0xffe4e5e7);   // testi chiari della barra di servizio
    const juce::Colour stripCol   (0xff121214);
    const juce::Colour lineCol    (0xff36373b);
    const juce::Colour learnCol   (0xffffb020);
    const juce::Colour accent     (0xff3f8f6a);   // evidenziazioni nei menu

    juce::Font uiFont (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font faceFont (juce::Typeface::Ptr face, float pt, float tracking = 0.0f)
    {
        return juce::Font (juce::FontOptions (face).withPointHeight (pt)).withExtraKerningFactor (tracking / pt);
    }

    const char* displayNames[] = { "SPEED", "DEPTH", "FORMA D'ONDA", "FILTER / ENVELOPE", "IN / OUT",
                                   "INPUT", "OUTPUT", "VOLUME CHITARRA",
                                   "VR2 TRIANGLE SHAPING", "VR3 TRIANGLE OFFSET", "VR5 SET BIAS", "VR6 LEVEL",
                                   "BATTERIE", "2N3819 Vp" };

    void drawBaseline (juce::Graphics& g, const juce::Font& f, const juce::String& s,
                       float x, float baseline, juce::Justification j)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, s);
        if (j.testFlags (juce::Justification::horizontallyCentred)) x -= w * 0.5f;
        else if (j.testFlags (juce::Justification::right)) x -= w;
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, x, baseline);
        ga.draw (g);
    }

    juce::ColourGradient chrome (juce::Point<float> c, float r, float darken = 0.0f)
    {
        const float d = 2.0f * r;
        juce::ColourGradient cg (juce::Colours::white.darker (darken), c.x - 0.15f * d, c.y - 0.2f * d,
                                 juce::Colour (0xff55595e).darker (darken), c.x - 0.15f * d + 0.85f * d, c.y - 0.2f * d, true);
        cg.addColour (0.35, juce::Colour (0xffd6d8db).darker (darken));
        cg.addColour (0.75, juce::Colour (0xff868a90).darker (darken));
        return cg;
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
    {
        using namespace juce;
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (Rectangle<float> (2 * r + 2, 2 * r + 2).withCentre (c.translated (0.5f, 1.2f)));
        g.setGradientFill (chrome (c, r, 0.1f));
        g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
        g.setColour (Colour (0xff6d6a60));
        g.drawEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c), 0.6f);
        g.setColour (Colour (0xff4a4a48));
        const float a = r * 0.62f;
        g.drawLine (c.x - a, c.y - a * 0.3f, c.x + a, c.y + a * 0.3f, r * 0.28f);
        g.drawLine (c.x - a * 0.3f, c.y + a, c.x + a * 0.3f, c.y - a, r * 0.28f);
    }

    // simboli delle forme d'onda (S1), centrati in c, larghezza w
    void drawWaveSymbol (juce::Graphics& g, int wave, juce::Point<float> c, float w, float stroke)
    {
        using namespace juce;
        const float h = w * 0.42f, x0 = c.x - w * 0.5f, y0 = c.y + h * 0.5f, y1 = c.y - h * 0.5f, p = w * 0.5f;
        Path s;
        switch (wave)
        {
            case 0:   // rampa che sale
                s.startNewSubPath (x0, y0);
                s.lineTo (x0 + p, y1); s.lineTo (x0 + p, y0); s.lineTo (x0 + 2 * p, y1); s.lineTo (x0 + 2 * p, y0);
                break;
            case 1:   // rampa che scende
                s.startNewSubPath (x0, y0);
                s.lineTo (x0, y1); s.lineTo (x0 + p, y0); s.lineTo (x0 + p, y1); s.lineTo (x0 + 2 * p, y0);
                break;
            case 2:   // triangolo
                s.startNewSubPath (x0, y0);
                s.lineTo (x0 + p * 0.5f, y1); s.lineTo (x0 + p, y0); s.lineTo (x0 + p * 1.5f, y1); s.lineTo (x0 + 2 * p, y0);
                break;
            default:  // quadra
                s.startNewSubPath (x0, y0);
                s.lineTo (x0, y1); s.lineTo (x0 + p * 0.5f, y1); s.lineTo (x0 + p * 0.5f, y0); s.lineTo (x0 + p, y0);
                s.lineTo (x0 + p, y1); s.lineTo (x0 + p * 1.5f, y1); s.lineTo (x0 + p * 1.5f, y0); s.lineTo (x0 + 2 * p, y0);
                break;
        }
        g.strokePath (s, PathStrokeType (stroke, PathStrokeType::mitered, PathStrokeType::square));
    }

    // angoli delle quattro posizioni di S1 (radianti, 0 = in alto, orario positivo)
    constexpr float waveStart = juce::MathConstants<float>::twoPi - 1.0472f;   // −60°
    constexpr float waveEnd   = juce::MathConstants<float>::twoPi + 1.0472f;   // +60°

    // frequenza centrale del filtro e guadagno del VCA per una tensione di gate (pannello interno)
    double rdsFor (double vg, double vp)
    {
        const double beta = grz::JFET::beta * (grz::JFET::vto / vp), ov = vg - vp;
        return ov <= 0.0 ? 1e12 : 1.0 / (2.0 * beta * ov);
    }
    double filterHz (double vg, double vp)
    {
        using C = grz::Components;
        const double rds = rdsFor (vg, vp), rp = 1.0 / (1.0 / C::R22 + 1.0 / rds);
        return 1.0 / (2.0 * grz::kPi * C::C8 * std::sqrt (rp * C::R24));
    }
    double vcaDb (double vg, double vp)
    {
        using C = grz::Components;
        const double rp = 1.0 / (1.0 / rdsFor (vg, vp) + 1.0 / C::R23);
        return 20.0 * std::log10 (11.0 * rp / (C::R22 + rp) * C::R24 / C::R23);
    }
    juce::String hzText (double f)
    {
        return f >= 1000.0 ? juce::String (f / 1000.0, 2).replaceCharacter ('.', ',') + " kHz"
                           : juce::String (juce::roundToInt (f)) + " Hz";
    }
}

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    using namespace juce;
    setColour (PopupMenu::backgroundColourId, Colour (0xff1c1c1e));
    setColour (PopupMenu::textColourId, cream);
    setColour (PopupMenu::highlightedBackgroundColourId, accent);
    setColour (BubbleComponent::backgroundColourId, Colour (0xee111111));
    setColour (BubbleComponent::outlineColourId, cream.withAlpha (0.4f));
    setColour (TooltipWindow::textColourId, cream);
    setColour (ComboBox::backgroundColourId, Colour (0xff1c1c1e));
    setColour (ComboBox::outlineColourId, lineCol);
    setColour (ComboBox::textColourId, cream);
    setColour (ComboBox::arrowColourId, cream.withAlpha (0.7f));
    setColour (TextButton::buttonColourId, Colour (0xff1c1c1e));
    setColour (TextButton::textColourOffId, cream);
}

void PedalLookAndFeel::drawPedalKnob (juce::Graphics& g, juce::Point<float> c, float k, float angle)
{
    using namespace juce;
    // ombra portata sul pannello
    g.setColour (Colours::black.withAlpha (0.22f));
    g.fillEllipse (Rectangle<float> (96 * k, 96 * k).withCentre (c.translated (4 * k, 7 * k)));
    g.setColour (Colours::black.withAlpha (0.12f));
    g.fillEllipse (Rectangle<float> (104 * k, 104 * k).withCentre (c.translated (6 * k, 10 * k)));

    // fianco del cilindro
    g.setGradientFill (ColourGradient (Colour (0xff2e2e30), c.x, c.y - 46 * k, Colour (0xff040404), c.x, c.y + 48 * k, false));
    g.fillEllipse (Rectangle<float> (92 * k, 92 * k).withCentre (c.translated (0, 3 * k)));
    // gonna zigrinata
    g.setColour (Colour (0xff3a3a3d));
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i / 48.0f * MathConstants<float>::twoPi;
        g.drawLine (Line<float> (c.getPointOnCircumference (40 * k, a), c.getPointOnCircumference (45.5f * k, a)), 0.9f * k);
    }
    ColourGradient bevel (Colour (0xff4a4a4e), c.x - 26 * k, c.y - 36 * k, Colour (0xff070707), c.x + 26 * k, c.y + 36 * k, false);
    g.setGradientFill (bevel);
    g.fillEllipse (Rectangle<float> (78 * k, 78 * k).withCentre (c));

    // cappellotto chiaro con la croce (ruota con la manopola)
    const float r = 30.0f * k;
    g.setGradientFill (ColourGradient (Colour (0xfff2f2ef), c.x - r * 0.6f, c.y - r * 0.7f, Colour (0xffa9aaa6), c.x + r * 0.7f, c.y + r * 0.8f, true));
    g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c), 1.0f * k);
    // riflesso lucido fisso
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), c.x - 10 * k, c.y - 20 * k,
                                       Colours::white.withAlpha (0.0f), c.x - 10 * k, c.y + 2 * k, false));
    g.fillEllipse (Rectangle<float> (36 * k, 20 * k).withCentre (c.translated (-6 * k, -14 * k)));
    // la croce: quattro bracci neri; quello dell'indice arriva al bordo della gonna
    const auto rot = AffineTransform::rotation (angle).translated (c);
    g.setColour (Colour (0xff141414));
    for (int i = 0; i < 4; ++i)
    {
        Path arm; arm.addRoundedRectangle (-2.6f * k, -r + 3.0f * k, 5.2f * k, r - 6.0f * k, 1.6f * k);
        g.fillPath (arm, AffineTransform::rotation ((float) i * MathConstants<float>::halfPi).followedBy (rot));
    }
    g.fillEllipse (Rectangle<float> (8 * k, 8 * k).withCentre (c));
    // indice bianco sulla gonna
    Path idx; idx.addRoundedRectangle (-2.0f * k, -45.0f * k, 4.0f * k, 12.0f * k, 1.8f * k);
    g.setColour (Colour (0xfff4f4f2));
    g.fillPath (idx, rot);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto style = slider.getProperties()["style"].toString();

    if (style == "trimmer")   // trimmer da circuito stampato (blu, regolabile col cacciavite)
    {
        const float side = jmin (b.getWidth(), b.getHeight()) * 0.62f, r = side * 0.36f;
        const auto sq = Rectangle<float> (side, side).withCentre (c);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (sq.translated (0, 2.0f), side * 0.08f);
        g.setGradientFill (ColourGradient (Colour (0xff3d6fd1), sq.getX(), sq.getY(), Colour (0xff1c3c86), sq.getRight(), sq.getBottom(), false));
        g.fillRoundedRectangle (sq, side * 0.08f);
        g.setColour (Colour (0xffd9dde4));
        g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
        Path slot; slot.addRoundedRectangle (-r * 0.18f, -r * 0.9f, r * 0.36f, r * 1.8f, r * 0.1f);
        g.setColour (Colour (0xff3a3d42));
        g.fillPath (slot, AffineTransform::rotation (angle).translated (c));
        g.setColour (Colour (0xffc9ccd0).withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (c.getPointOnCircumference (side * 0.74f, a)));
        }
        return;
    }

    if (style == "charge")    // tensione delle batterie: scala rossa / ambra / verde
    {
        const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        auto arc = [&] (float from, float to, Colour col)
        {
            Path a; a.addCentredArc (c.x, c.y, R * 0.86f, R * 0.86f, 0.0f,
                                     startAngle + from * (endAngle - startAngle), startAngle + to * (endAngle - startAngle), true);
            g.setColour (col);
            g.strokePath (a, PathStrokeType (R * 0.1f, PathStrokeType::curved, PathStrokeType::butt));
        };
        const float span = (float) (Battery::fullV - Battery::minV);
        const float red = (float) ((7.4 - Battery::minV) / span), amber = (float) ((8.1 - Battery::minV) / span);
        arc (0.0f, red, Colour (0xffc8302b));
        arc (red, amber, Colour (0xffd9a32c));
        arc (amber, 1.0f, Colour (0xff6fae55));
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillEllipse (Rectangle<float> (R * 1.36f, R * 1.36f).withCentre (c.translated (0, R * 0.06f)));
        g.setGradientFill (ColourGradient (Colour (0xff3b3b3e), c.x - R * 0.2f, c.y - R * 0.3f, Colour (0xff050505), c.x + R * 0.4f, c.y + R * 0.5f, true));
        g.fillEllipse (Rectangle<float> (R * 1.3f, R * 1.3f).withCentre (c));
        Path p; p.addRoundedRectangle (-R * 0.05f, -R * 0.62f, R * 0.1f, R * 0.36f, R * 0.05f);
        g.setColour (Colour (0xfff1f2f3));
        g.fillPath (p, AffineTransform::rotation (angle).translated (c));
        return;
    }

    if (style == "pedal")     // manopole del pedale
    {
        drawPedalKnob (g, c, jmin (b.getWidth(), b.getHeight()) / 126.0f, angle);
        return;
    }

    // manopole di servizio (INPUT / OUTPUT / VOLUME)
    const float k = jmin (b.getWidth(), b.getHeight()) / 34.0f;
    g.setColour (Colour (0xff1c1c1e)); g.fillEllipse (Rectangle<float> (30 * k, 30 * k).withCentre (c));
    g.setColour (Colour (0xff4a4a50)); g.drawEllipse (Rectangle<float> (30 * k, 30 * k).withCentre (c), 1.0f);
    g.setColour (Colour (0xff2b2b2f)); g.fillEllipse (Rectangle<float> (18 * k, 18 * k).withCentre (c));
    Path p; p.addRoundedRectangle (-1.2f * k, -14.0f * k, 2.4f * k, 8.0f * k, 1.2f * k);
    g.setColour (Colour (0xffd9dbde));
    g.fillPath (p, AffineTransform::rotation (angle).translated (c));
}

//==============================================================================
void LearnableSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Slider::mouseDown (e);
}

void FootSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Button::mouseDown (e);
}

void FootSwitch::drawStomp (juce::Graphics& g, juce::Rectangle<float> b, bool down, bool highlighted)
{
    using namespace juce;
    const auto c = b.getCentre();
    const float k = jmin (b.getWidth(), b.getHeight()) / 62.0f;
    // ombra
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (48 * k, 48 * k).withCentre (c.translated (3 * k, 5 * k)));
    // dado esagonale
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = c.getPointOnCircumference (25.0f * k, (float) i * MathConstants<float>::pi / 3.0f + 0.2f);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setGradientFill (chrome (c, 25 * k, 0.15f));
    g.fillPath (hex);
    g.setColour (Colour (0xff5d6066));
    g.strokePath (hex, PathStrokeType (0.8f * k));
    // rondella e gambo
    g.setGradientFill (chrome (c, 18 * k, 0.05f));
    g.fillEllipse (Rectangle<float> (34 * k, 34 * k).withCentre (c));
    g.setColour (Colour (0xff6d7076));
    g.drawEllipse (Rectangle<float> (34 * k, 34 * k).withCentre (c), 0.8f * k);
    // pulsante (si abbassa quando è premuto)
    const auto cap = c.translated (0, down ? 1.5f * k : 0.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (24 * k, 24 * k).withCentre (cap.translated (0, 2 * k)));
    g.setGradientFill (chrome (cap, 12 * k, down ? 0.2f : 0.0f));
    g.fillEllipse (Rectangle<float> (23 * k, 23 * k).withCentre (cap));
    g.setColour (Colours::white.withAlpha (0.7f));
    g.fillEllipse (Rectangle<float> (8 * k, 5 * k).withCentre (cap.translated (-4 * k, -5 * k)));
    if (highlighted) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillEllipse (Rectangle<float> (34 * k, 34 * k).withCentre (c)); }
}

void FootSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    drawStomp (g, getLocalBounds().toFloat(), down, highlighted);
}

//==============================================================================
InteriorPanel::InteriorPanel (GristleProcessor& p, juce::Typeface::Ptr panelFace,
                              std::function<void (int, juce::Component*)> learnMenu)
    : proc (p), face (panelFace)
{
    using namespace juce;
    auto setupKnob = [&] (LearnableSlider& s, const char* id, const char* style, double def, const char* tip)
    {
        s.setSliderStyle (Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (400);
        s.getProperties().set ("style", style);
        s.setPopupDisplayEnabled (true, true, this);
        const int idx = proc.getLearnIndex (id);
        s.onLearnMenu = [learnMenu, idx, &s] { learnMenu (idx, &s); };
        s.setTooltip (String::fromUTF8 (tip) + ". Clic destro: MIDI Learn. Doppio clic: taratura originale. Maiusc + trascina: regolazione fine");
        s.setDoubleClickReturnValue (true, def);
        s.setVelocityModeParameters (1.0, 1, 0.0, false, ModifierKeys::shiftModifier);
        addAndMakeVisible (s);
    };
    setupKnob (vr2, ParamIDs::vr2, "trimmer", grz::Trim::vr2, "VR2 TRIANGLE SHAPING: sposta il punto in cui TR1 satura (la punta del triangolo)");
    setupKnob (vr3, ParamIDs::vr3, "trimmer", grz::Trim::vr3, "VR3 TRIANGLE OFFSET: toglie la componente continua del triangolo");
    setupKnob (vr5, ParamIDs::vr5, "trimmer", grz::Trim::vr5, "VR5 SET BIAS: polarizzazione del gate del JFET, cio\xc3\xa8 il centro della spazzata");
    setupKnob (vr6, ParamIDs::vr6, "trimmer", grz::Trim::vr6, "VR6 LEVEL: livello d'uscita dell'effetto");
    setupKnob (battery, ParamIDs::battery, "charge", Battery::nominalV, "Tensione delle due PP3 (\xc2\xb1V)");
    setupKnob (vp, ParamIDs::jfetVp, "trimmer", -3.0, "Vgs(off) del 2N3819 (da \xe2\x88\x92" "0,5 a \xe2\x88\x92" "8 V secondo il datasheet): cambia il JFET, poi ritocca VR5");
    vr6.setSkewFactorFromMidPoint (0.08);
    vr2A = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::vr2, vr2);
    vr3A = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::vr3, vr3);
    vr5A = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::vr5, vr5);
    vr6A = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::vr6, vr6);
    batA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::battery, battery);
    vpA  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::jfetVp, vp);
    vr6.setSkewFactorFromMidPoint (0.08);   // il livello giusto sta tutto all'inizio della corsa

    resetBtn.setTooltip ("Trimmer come nella taratura dell'articolo, batterie a 9 V, 2N3819 tipico (Vp \xe2\x88\x92" "3 V)");
    resetBtn.onClick = [this] { resetToOriginal(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (resetBtn);
    addAndMakeVisible (closeBtn);
    startTimerHz (30);
}

juce::Font InteriorPanel::font (float pt, float tracking) const
{
    return juce::Font (juce::FontOptions (face).withPointHeight (pt)).withExtraKerningFactor (tracking / pt);
}

juce::Rectangle<int> InteriorPanel::area (float x, float y, float w, float h) const
{
    return juce::Rectangle<float> (x * k, y * k, w * k, h * k).toNearestInt();
}

void InteriorPanel::resized()
{
    k = (float) getWidth() / baseW;
    const float y = 318.0f;
    vr2.setBounds (area (24, y - 40, 80, 80));
    vr3.setBounds (area (107, y - 40, 80, 80));
    vr5.setBounds (area (190, y - 40, 80, 80));
    vr6.setBounds (area (273, y - 40, 80, 80));
    battery.setBounds (area (40, 448, 92, 92));
    vp.setBounds (area (250, 452, 84, 84));
    resetBtn.setBounds (area (24, 600, 180, 34));
    closeBtn.setBounds (area (276, 600, 80, 34));
}

juce::Component* InteriorPanel::controlFor (int idx)
{
    switch (idx)
    {
        case GristleProcessor::idxVr2: return &vr2;
        case GristleProcessor::idxVr3: return &vr3;
        case GristleProcessor::idxVr5: return &vr5;
        case GristleProcessor::idxVr6: return &vr6;
        case GristleProcessor::idxBattery: return &battery;
        case GristleProcessor::idxVp: return &vp;
        default: return nullptr;
    }
}

void InteriorPanel::resetToOriginal()
{
    auto set = [this] (const char* id, float v)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture();
    };
    set (ParamIDs::vr2, (float) grz::Trim::vr2);
    set (ParamIDs::vr3, (float) grz::Trim::vr3);
    set (ParamIDs::vr5, (float) grz::Trim::vr5);
    set (ParamIDs::vr6, (float) grz::Trim::vr6);
    set (ParamIDs::battery, (float) Battery::nominalV);
    set (ParamIDs::jfetVp, -3.0f);
}

void InteriorPanel::timerCallback()
{
    if (isVisible()) repaint();
}

void InteriorPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    g.addTransform (AffineTransform::scale (k));
    const float W = baseW, H = baseH;

    g.setColour (Colour (0xff141416));
    g.fillRoundedRectangle (0, 0, W, H, 14.0f);
    g.setColour (Colour (0xff3a3a3e));
    g.drawRoundedRectangle (0.5f, 0.5f, W - 1.0f, H - 1.0f, 14.0f, 1.0f);

    g.setColour (cream);
    g.setFont (font (18.0f, 4.0f));
    g.drawText ("INTERNO", Rectangle<float> (0, 16, W, 24), Justification::centred);
    g.setColour (Colour (0xff8a8d92));
    g.setFont (uiFont (10.0f, false));
    g.drawText ("taratura (\"Setting up\"), batterie e JFET", Rectangle<float> (0, 40, W, 14), Justification::centred);

    // --- oscilloscopio sulla forma d'onda (cima di VR4) ---
    const Rectangle<float> scr (24, 62, W - 48, 128);
    g.setColour (Colour (0xff07120c));
    g.fillRoundedRectangle (scr, 6.0f);
    g.setColour (Colour (0xff1d3a28));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine (roundToInt (scr.getX() + scr.getWidth() * (float) i / 8.0f), scr.getY() + 2, scr.getBottom() - 2);
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine (roundToInt (scr.getY() + scr.getHeight() * (float) i / 4.0f), scr.getX() + 2, scr.getRight() - 2);
    const float vPerDiv = 2.0f;                         // 2 V per divisione, ±4 V
    const float hz = proc.lfoHz.load(), rate = proc.scopeRate.load();
    const bool live = proc.running.load();
    if (live && rate > 0.0f)
    {
        const float span = jlimit (0.02f, 2.6f, hz > 0.05f ? 2.2f / hz : 2.6f);
        const int n = jlimit (16, GristleProcessor::scopeSize - 1, (int) (span * rate));
        const int w = proc.scopeWrite.load (std::memory_order_acquire);
        Path tr;
        const int cols = (int) scr.getWidth();
        for (int c = 0; c <= cols; ++c)
        {
            const int idx = ((w - n + (int) ((float) c / (float) cols * (float) (n - 1))) % GristleProcessor::scopeSize + GristleProcessor::scopeSize) % GristleProcessor::scopeSize;
            const float v = proc.scope[(size_t) idx].load (std::memory_order_relaxed);
            const float yy = jlimit (scr.getY() + 2, scr.getBottom() - 2, scr.getCentreY() - v / vPerDiv * scr.getHeight() / 4.0f);
            if (c == 0) tr.startNewSubPath (scr.getX() + (float) c, yy); else tr.lineTo (scr.getX() + (float) c, yy);
        }
        g.setColour (Colour (0xff7dff9a).withAlpha (0.25f));
        g.strokePath (tr, PathStrokeType (3.0f));
        g.setColour (Colour (0xffa6ffb8));
        g.strokePath (tr, PathStrokeType (1.2f));
    }
    g.setColour (Colour (0xff6fbf86));
    g.setFont (uiFont (9.0f, false));
    g.drawText ("forma d'onda su VR4 (DEPTH)   2 V/div", scr.reduced (8, 4), Justification::topLeft);
    g.drawText (! live ? String ("pedale su OUT") : (hz > 0.0f ? "LFO " + String (hz, hz < 10 ? 2 : 1).replaceCharacter ('.', ',') + " Hz" : String ("LFO fermo")),
                scr.reduced (8, 4), Justification::topRight);

    // --- spazzata misurata ---
    {
        const float gmin = proc.gateMin.load(), gmax = proc.gateMax.load();
        const double vpv = proc.apvts.getRawParameterValue (ParamIDs::jfetVp)->load();
        const bool env = proc.apvts.getRawParameterValue (ParamIDs::mode)->load() > 0.5f;
        g.setColour (cream);
        g.setFont (uiFont (11.0f));
        String l1 = String::fromUTF8 ("gate del JFET: da ") + String (gmin, 2) + " a " + String (gmax, 2) + " V";
        String l2;
        if (! env)
            l2 = "filtro: da " + hzText (filterHz (gmin, vpv)) + " a " + hzText (filterHz (gmax, vpv));
        else
            l2 = "VCA: da " + String (vcaDb (gmax, vpv), 1) + " a " + String (vcaDb (gmin, vpv), 1) + " dB";
        if (gmax > -0.05f) l2 << String::fromUTF8 ("   (gate al limite: VR5 troppo alto)");
        else if (gmin < vpv) l2 << String::fromUTF8 ("   (JFET interdetto in fondo alla corsa)");
        g.drawText (live ? l1 : String(), Rectangle<float> (24, 196, W - 48, 16), Justification::centredLeft);
        g.setColour (Colour (0xffb9bcc0));
        g.setFont (uiFont (10.5f, false));
        g.drawText (live ? l2 : String(), Rectangle<float> (24, 212, W - 48, 16), Justification::centredLeft);
    }

    // --- basetta con i quattro trimmer ---
    const Rectangle<float> pcb (16, 236, W - 32, 168);
    g.setColour (Colour (0xffc98a3e));
    g.fillRoundedRectangle (pcb, 6.0f);
    g.setColour (Colour (0xffe6b06a));
    for (float yy = pcb.getY() + 10; yy < pcb.getBottom() - 6; yy += 15.0f)
        g.fillRect (pcb.getX() + 6, yy, pcb.getWidth() - 12, 9.0f);
    g.setColour (Colour (0xff5a3a14).withAlpha (0.55f));
    for (float yy = pcb.getY() + 14.5f; yy < pcb.getBottom() - 6; yy += 15.0f)
        for (float xx = pcb.getX() + 12; xx < pcb.getRight() - 8; xx += 15.0f)
            g.fillEllipse (xx - 1.6f, yy - 1.6f, 3.2f, 3.2f);

    struct T { float cx; const char* a; const char* b; const char* id; };
    const T ts[] = { { 64.0f, "VR2", "TRIANGLE\nSHAPING", ParamIDs::vr2 }, { 147.0f, "VR3", "TRIANGLE\nOFFSET", ParamIDs::vr3 },
                     { 230.0f, "VR5", "SET\nBIAS", ParamIDs::vr5 },          { 313.0f, "VR6", "LEVEL", ParamIDs::vr6 } };
    for (auto& t : ts)
    {
        g.setColour (Colour (0xcc111012));
        g.fillRoundedRectangle (t.cx - 38, 244, 76, 32, 4.0f);
        g.setColour (Colour (0xffeee8f0));
        g.setFont (font (10.0f, 1.0f));
        g.drawText (t.a, Rectangle<float> (t.cx - 38, 245, 76, 12), Justification::centred);
        g.setFont (uiFont (8.0f));
        g.drawFittedText (t.b, Rectangle<float> (t.cx - 38, 256, 76, 20).toNearestInt(), Justification::centred, 2);
        const float v = proc.apvts.getRawParameterValue (t.id)->load();
        String val = String (v * 100.0f, 1) + " %";
        if (String (t.id) == ParamIDs::vr6) val = String (Decibels::gainToDecibels (v, -80.0f), 1) + " dB";
        g.setColour (Colour (0xcc111012));
        g.fillRoundedRectangle (t.cx - 38, 364, 76, 20, 4.0f);
        g.setColour (Colour (0xffeee8f0));
        g.setFont (uiFont (10.5f));
        g.drawText (val, Rectangle<float> (t.cx - 38, 366, 76, 16), Justification::centred);
    }
    g.setColour (Colour (0xcc111012));
    g.fillRoundedRectangle (pcb.getX() + 8, 386, pcb.getWidth() - 16, 14, 3.0f);
    g.setColour (Colour (0xffcfc6d2));
    g.setFont (uiFont (8.5f, false));
    g.drawText (String::fromUTF8 ("VR2 triangolo senza salti \xc2\xb7 VR3 0 V medi \xc2\xb7 VR5 spazzata piena \xc2\xb7 VR6 stesso volume"),
                Rectangle<float> (pcb.getX() + 8, 387, pcb.getWidth() - 16, 12), Justification::centred);

    // --- batterie e JFET ---
    const float vb = proc.apvts.getRawParameterValue (ParamIDs::battery)->load();
    const float vpv = proc.apvts.getRawParameterValue (ParamIDs::jfetVp)->load();
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (font (11.0f, 2.0f));
    g.drawText ("BATTERIE", Rectangle<float> (24, 420, 140, 16), Justification::centred);
    g.drawText ("JFET 2N3819", Rectangle<float> (220, 420, 144, 16), Justification::centred);
    g.setColour (cream);
    g.setFont (uiFont (12.0f));
    g.drawText (String::fromUTF8 ("2 \xc3\x97 PP3: \xc2\xb1") + String (vb, 2) + " V", Rectangle<float> (14, 544, 160, 16), Justification::centred);
    g.drawText ("Vp " + String (vpv, 2) + " V", Rectangle<float> (220, 544, 144, 16), Justification::centred);
    g.setColour (Colour (0xff9a9da2));
    g.setFont (uiFont (9.5f, false));
    g.drawText (String (roundToInt (Battery::chargeFromVolts (vb) * 100.0)) + String::fromUTF8 ("% di carica"), Rectangle<float> (14, 560, 160, 14), Justification::centred);
    const double idss = grz::JFET::beta * (grz::JFET::vto / (double) vpv) * (double) vpv * (double) vpv;
    g.drawText ("IDSS " + String (idss * 1000.0, 1) + " mA", Rectangle<float> (220, 560, 144, 14), Justification::centred);
    g.setColour (Colour (0xff6c6f74));
    g.setFont (uiFont (8.0f, false));
    g.drawText ("6 V", Rectangle<float> (30, 528, 40, 10), Justification::centredLeft);
    g.drawText ("9,6 V", Rectangle<float> (100, 528, 40, 10), Justification::centredRight);
}

//==============================================================================
// Il pedale, in unità di progetto: corpo largo 330 e alto 540 (u = scala)
void GristleEditor::drawPedal (juce::Graphics& g, juce::Rectangle<float> body, juce::Typeface::Ptr panelFace,
                               juce::Typeface::Ptr scriptFace, bool withExtras, int wave, bool envelope, bool out)
{
    using namespace juce;
    const float u = body.getWidth() / 330.0f;
    auto P = [&] (float x, float y) { return Point<float> (body.getX() + x * u, body.getY() + y * u); };
    auto F = [&] (float pt, float tr = 0.0f) { return faceFont (panelFace, pt * u, tr * u); };

    if (withExtras)
    {
        // i due cavi che escono dalle prese sul lato corto in alto (instrument / amplifier)
        for (float xx : { 86.0f, 244.0f })
        {
            const auto j = P (xx, 0);
            Path c; c.startNewSubPath (j.translated (0, -22 * u));
            c.cubicTo (j.translated (0, -50 * u), j.translated ((xx < 165 ? -20.0f : 20.0f) * u, -70 * u), j.translated ((xx < 165 ? -26.0f : 30.0f) * u, -120 * u));
            g.setColour (Colour (0xff060606));
            g.strokePath (c, PathStrokeType (12 * u, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Colour (0xff35353a));
            g.strokePath (c, PathStrokeType (2.6f * u, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (-3 * u, 0));
            // spinotto
            const auto plug = Rectangle<float> (22 * u, 30 * u).withCentre (j.translated (0, -16 * u));
            g.setGradientFill (ColourGradient (Colour (0xff3a3a3e), plug.getX(), 0, Colour (0xff0a0a0b), plug.getRight(), 0, false));
            g.fillRoundedRectangle (plug, 4 * u);
            g.setGradientFill (chrome (j.translated (0, -2 * u), 13 * u, 0.1f));
            g.fillRect (Rectangle<float> (26 * u, 7 * u).withCentre (j.translated (0, -3 * u)));
        }
    }

    // scatola di plastica nera (si vede il bordo) con il pannello chiaro avvitato sopra
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (body.translated (4 * u, 8 * u), 12 * u);
    g.setGradientFill (ColourGradient (Colour (0xff2a2a2c), body.getX(), body.getY(), Colour (0xff050505), body.getRight(), body.getBottom(), false));
    g.fillRoundedRectangle (body, 12 * u);
    g.setColour (Colour (0xff3c3c40));
    g.drawRoundedRectangle (body.reduced (0.6f * u), 12 * u, 1.0f * u);

    const auto plate = body.reduced (11 * u);
    ColourGradient pg (plateLight, plate.getX(), plate.getY(), plateDark, plate.getRight(), plate.getBottom(), false);
    pg.addColour (0.5, plateMid);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (plate, 4 * u);
    // leggera grana della vernice
    {
        Random rnd (7);
        for (int i = 0; i < 900; ++i)
        {
            const float xx = plate.getX() + rnd.nextFloat() * plate.getWidth(), yy = plate.getY() + rnd.nextFloat() * plate.getHeight();
            g.setColour ((rnd.nextBool() ? Colours::black : Colours::white).withAlpha (0.035f));
            g.fillRect (xx, yy, 1.2f * u, 1.2f * u);
        }
    }
    g.setColour (Colours::white.withAlpha (0.7f));
    g.drawLine (plate.getX() + 4 * u, plate.getY() + 0.8f * u, plate.getRight() - 4 * u, plate.getY() + 0.8f * u, 1.0f * u);
    g.setColour (Colour (0xff8f8b80));
    g.drawRoundedRectangle (plate, 4 * u, 0.8f * u);
    for (auto s : { P (24, 24), P (306, 24), P (24, 516), P (306, 516) }) drawScrew (g, s, 4.2f * u);

    g.setColour (ink);
    // prese: "instrument" (SK1, ingresso) e "amplifier" (SK2, uscita)
    drawBaseline (g, F (13.0f, 0.4f), "instrument", P (86, 0).x, P (0, 46).y, Justification::horizontallyCentred);
    drawBaseline (g, F (13.0f, 0.4f), "amplifier",  P (244, 0).x, P (0, 46).y, Justification::horizontallyCentred);

    // manopole
    drawBaseline (g, F (13.5f, 0.4f), "speed", P (speedX - 22, 0).x, P (0, speedY - 62).y, Justification::horizontallyCentred);
    drawBaseline (g, F (13.5f, 0.4f), "depth", P (depthX + 6, 0).x,  P (0, depthY - 62).y, Justification::horizontallyCentred);

    // posizioni di S1: i quattro simboli sull'arco, il selezionato pieno
    for (int i = 0; i < 4; ++i)
    {
        const float a = waveStart + (float) i / 3.0f * (waveEnd - waveStart);
        const auto c = Point<float> (P (waveX, waveY)).getPointOnCircumference (74.0f * u, a);
        g.setColour (ink.withAlpha (i == wave ? 1.0f : 0.55f));
        drawWaveSymbol (g, i, c, 24.0f * u, (i == wave ? 2.0f : 1.4f) * u);
        // tacca della posizione
        g.drawLine (Line<float> (Point<float> (P (waveX, waveY)).getPointOnCircumference (60.0f * u, a),
                                 Point<float> (P (waveX, waveY)).getPointOnCircumference (65.0f * u, a)), 1.4f * u);
    }

    // interruttori a pedale: la parola della posizione attiva è piena, l'altra attenuata
    {
        g.setColour (ink.withAlpha (envelope ? 0.45f : 1.0f));
        drawBaseline (g, F (13.0f, 0.4f), "filter",   P (modeX, 0).x, P (0, modeY - 36).y, Justification::horizontallyCentred);
        g.setColour (ink.withAlpha (envelope ? 1.0f : 0.45f));
        drawBaseline (g, F (13.0f, 0.4f), "envelope", P (modeX, 0).x, P (0, modeY + 48).y, Justification::horizontallyCentred);
        g.setColour (ink.withAlpha (out ? 0.45f : 1.0f));
        drawBaseline (g, F (13.0f, 0.4f), "in",  P (inX, 0).x, P (0, inY - 36).y, Justification::horizontallyCentred);
        g.setColour (ink.withAlpha (out ? 1.0f : 0.45f));
        drawBaseline (g, F (13.0f, 0.4f), "out", P (inX, 0).x, P (0, inY + 48).y, Justification::horizontallyCentred);
    }

    // marchio (in basso a destra) e nome del plugin sotto le prese
    g.setColour (ink.withAlpha (0.85f));
    if (scriptFace != nullptr)
        drawBaseline (g, Font (FontOptions (scriptFace).withPointHeight (20.0f * u)), "Collateral", P (288, 0).x, P (0, 498).y, Justification::right);
    drawBaseline (g, F (7.5f, 2.2f), "EFFECTS", P (288, 0).x, P (0, 511).y, Justification::right);
    g.setColour (ink.withAlpha (0.5f));
    drawBaseline (g, F (8.5f, 2.6f), "cartilagine", P (42, 0).x, P (0, 511).y, Justification::left);
}

juce::Image GristleEditor::createAppIcon (int size)
{
    using namespace juce;
    Image img (Image::ARGB, size, size, true);
    Graphics g (img);
    const float u = (float) size / 1024.0f;
    const auto body = Rectangle<float> (100 * u, 100 * u, 824 * u, 824 * u);
    Path bodyPath; bodyPath.addRoundedRectangle (body, 185.0f * u);
    DropShadow (Colours::black.withAlpha (0.5f), roundToInt (28 * u), { 0, roundToInt (12 * u) }).drawForPath (g, bodyPath);
    g.setGradientFill (ColourGradient (Colour (0xff2a2a2c), body.getX(), body.getY(), Colour (0xff050505), body.getRight(), body.getBottom(), false));
    g.fillPath (bodyPath);
    Path platePath; platePath.addRoundedRectangle (body.reduced (40 * u), 150.0f * u);
    ColourGradient pg (plateLight, body.getX(), body.getY(), plateDark, body.getRight(), body.getBottom(), false);
    pg.addColour (0.5, plateMid);
    g.setGradientFill (pg);
    g.fillPath (platePath);

    PedalLookAndFeel::drawPedalKnob (g, { 512 * u, 440 * u }, 3.4f * u, 0.7f);
    g.setColour (ink);
    for (int i = 0; i < 4; ++i)
    {
        const float a = waveStart + (float) i / 3.0f * (waveEnd - waveStart);
        drawWaveSymbol (g, i, Point<float> (512 * u, 440 * u).getPointOnCircumference (230 * u, a), 80 * u, 9 * u);
    }
    auto face = Typeface::createSystemTypefaceFor (BinaryData::QuestrialRegular_ttf, BinaryData::QuestrialRegular_ttfSize);
    drawBaseline (g, faceFont (face, 120.0f * u, 2.0f * u), "gristle", 512 * u, 830 * u, Justification::horizontallyCentred);
    return img;
}

//==============================================================================
GristleEditor::GristleEditor (GristleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    scriptFace = juce::Typeface::createSystemTypefaceFor (BinaryData::YellowtailRegular_ttf, BinaryData::YellowtailRegular_ttfSize);
    panelFace  = juce::Typeface::createSystemTypefaceFor (BinaryData::QuestrialRegular_ttf, BinaryData::QuestrialRegular_ttfSize);
    uiFace     = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);

    auto setupKnob = [this] (LearnableSlider& s, const char* id, bool small)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setPopupDisplayEnabled (true, true, this);
        auto* prm = proc.apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        s.setMouseDragSensitivity (small ? 160 : 240);
        if (! small) s.getProperties().set ("style", "pedal");
        s.setTooltip ("Clic destro: MIDI Learn. Doppio clic: valore predefinito");
        const int idx = proc.getLearnIndex (id);
        s.onLearnMenu = [this, idx, &s] { showLearnMenu (idx, &s); };
        addAndMakeVisible (s);
    };

    setupKnob (speed,   ParamIDs::speed,  false);
    setupKnob (depth,   ParamIDs::depth,  false);
    setupKnob (wave,    ParamIDs::wave,   false);
    setupKnob (inTrim,  ParamIDs::input,  true);
    setupKnob (outTrim, ParamIDs::output, true);
    setupKnob (gtrVol,  ParamIDs::gtrVol, true);
    wave.setRotaryParameters (waveStart, waveEnd, true);
    wave.setMouseDragSensitivity (120);
    speed.setTooltip (juce::String::fromUTF8 ("SPEED (VR1, 10k log): velocit\xc3\xa0 dell'oscillatore, da circa 0,1 a 90 Hz. Clic destro: MIDI Learn"));
    depth.setTooltip ("DEPTH (VR4, 10k lin): quanta forma d'onda arriva al gate del JFET. Clic destro: MIDI Learn");
    wave.setTooltip ("Forma d'onda (S1): rampa che sale, rampa che scende, triangolo, quadra. Clic destro: MIDI Learn");
    gtrVol.setTooltip ("Volume della chitarra (pot logaritmico). Clic destro: MIDI Learn");

    speedA = std::make_unique<SA> (proc.apvts, ParamIDs::speed,  speed);
    depthA = std::make_unique<SA> (proc.apvts, ParamIDs::depth,  depth);
    waveA  = std::make_unique<SA> (proc.apvts, ParamIDs::wave,   wave);
    inA    = std::make_unique<SA> (proc.apvts, ParamIDs::input,  inTrim);
    outA   = std::make_unique<SA> (proc.apvts, ParamIDs::output, outTrim);
    gtrA   = std::make_unique<SA> (proc.apvts, ParamIDs::gtrVol, gtrVol);

    modeSw.onLearnMenu   = [this] { showLearnMenu (GristleProcessor::idxMode, &modeSw); };
    bypassSw.onLearnMenu = [this] { showLearnMenu (GristleProcessor::idxBypass, &bypassSw); };
    modeSw.setTooltip ("FILTER / ENVELOPE (S2): filtro passa-banda che si sposta, oppure amplificatore controllato (tremolo). Clic destro: MIDI Learn");
    bypassSw.setTooltip ("IN / OUT (S3): effetto inserito o segnale diretto. Clic destro: MIDI Learn");
    addAndMakeVisible (modeSw);
    addAndMakeVisible (bypassSw);
    modeA   = std::make_unique<BA> (proc.apvts, ParamIDs::mode,   modeSw);
    bypassA = std::make_unique<BA> (proc.apvts, ParamIDs::bypass, bypassSw);

    pickupBox.setTooltip ("Pick-up della chitarra collegata (o sorgente di linea): l'ingresso del pedale (47 k\xce\xa9) lo carica");
    pickupBox.onChange = [this] { pickupChosen(); };
    addAndMakeVisible (pickupBox);
    pickupMenuBtn.setTooltip ("Salva i valori attuali con il nome della chitarra, o elimina una chitarra salvata");
    pickupMenuBtn.onClick = [this] { showPickupMenu(); };
    addAndMakeVisible (pickupMenuBtn);
    rebuildPickupBox();

    interior = std::make_unique<InteriorPanel> (proc, uiFace, [this] (int idx, juce::Component* c) { showLearnMenu (idx, c); });
    interior->onClose = [this] { interior->setVisible (false); interiorBtn.setToggleState (false, juce::dontSendNotification); };
    addChildComponent (*interior);
    interiorBtn.setClickingTogglesState (true);
    interiorBtn.setTooltip ("Apre l'interno del pedale: i quattro trimmer della taratura, le batterie, il JFET");
    interiorBtn.setColour (juce::TextButton::buttonOnColourId, accent);
    interiorBtn.onClick = [this] { interior->setVisible (interiorBtn.getToggleState()); interior->toFront (false); };
    addAndMakeVisible (interiorBtn);
    for (auto* s : { &pickR, &pickL })
    {
        s->setSliderStyle (juce::Slider::LinearBar);
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 54, 24);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::textBoxTextColourId, cream);
        s->setColour (juce::Slider::trackColourId, juce::Colour (0xff35483e));
        s->setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1c1c1e));
        s->setColour (juce::Slider::thumbColourId, cream);
        addAndMakeVisible (s);
    }
    pickR.setTooltip ("Resistenza in continua del pick-up (dato del costruttore)");
    pickL.setTooltip ("Induttanza del pick-up");
    pickRA = std::make_unique<SA> (proc.apvts, ParamIDs::pickupR, pickR);
    pickLA = std::make_unique<SA> (proc.apvts, ParamIDs::pickupL, pickL);
    pickR.setTextValueSuffix (juce::String::fromUTF8 (" k\xce\xa9"));
    pickL.setTextValueSuffix (" H");
    pickR.setNumDecimalPlacesToDisplay (1);
    pickL.setNumDecimalPlacesToDisplay (2);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    timerCallback();
    startTimerHz (30);
}

GristleEditor::~GristleEditor()
{
    proc.cancelLearn();
    setLookAndFeel (nullptr);
}

juce::Rectangle<float> GristleEditor::base (float x, float y, float w, float h) const
{
    return { x * scale, y * scale, w * scale, h * scale };
}

namespace { const juce::Rectangle<float> pedalBody (45.0f, GristleEditor::pedalY + 108.0f, 330.0f, 540.0f); }

void GristleEditor::resized()
{
    scale = (float) getWidth() / (float) baseW;
    auto place = [this] (juce::Component& c, float cx, float cy, float w, float h)
    { c.setBounds (base (cx - w * 0.5f, cy - h * 0.5f, w, h).toNearestInt()); };

    // barra di servizio
    place (inTrim,  40.0f,  25.0f, 34.0f, 34.0f);
    place (outTrim, 380.0f, 25.0f, 34.0f, 34.0f);
    pickupBox.setBounds     (base (62.0f,  64.0f, 132.0f, 24.0f).toNearestInt());
    pickupMenuBtn.setBounds (base (198.0f, 64.0f, 24.0f,  24.0f).toNearestInt());
    pickR.setBounds         (base (226.0f, 64.0f, 54.0f,  24.0f).toNearestInt());
    pickL.setBounds         (base (283.0f, 64.0f, 50.0f,  24.0f).toNearestInt());
    place (gtrVol, 396.0f, 76.0f, 28.0f, 28.0f);
    interiorBtn.setBounds   (base (298.0f, 12.0f, 52.0f,  36.0f).toNearestInt());
    if (interior != nullptr)
        interior->setBounds (base (20.0f, pedalY + 14.0f, InteriorPanel::baseW, InteriorPanel::baseH).toNearestInt());

    // pedale
    const float px = pedalBody.getX(), py = pedalBody.getY();
    place (speed,    px + speedX, py + speedY, knobSize, knobSize);
    place (depth,    px + depthX, py + depthY, knobSize, knobSize);
    place (wave,     px + waveX,  py + waveY,  knobSize, knobSize);
    place (modeSw,   px + modeX,  py + modeY,  switchSize, switchSize);
    place (bypassSw, px + inX,    py + inY,    switchSize, switchSize);

    renderBackground();
}

void GristleEditor::renderBackground()
{
    using namespace juce;
    background = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));

    // fondo: piano di lavoro scuro
    g.setGradientFill (ColourGradient (Colour (0xff1e1e20), 0, pedalY, Colour (0xff09090a), 0, (float) baseH, false));
    g.fillRect (0.0f, 0.0f, (float) baseW, (float) baseH);

    const bool env = proc.apvts.getRawParameterValue (ParamIDs::mode)->load() > 0.5f;
    const bool out = proc.apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    const int w = (int) proc.apvts.getRawParameterValue (ParamIDs::wave)->load();
    drawPedal (g, pedalBody, panelFace, scriptFace, true, w, env, out);

    // barra di servizio (non fa parte del pedale)
    g.setColour (stripCol);
    g.fillRect (0.0f, 0.0f, (float) baseW, 96.0f);
    g.setColour (lineCol);
    g.fillRect (0.0f, 95.0f, (float) baseW, 1.0f);
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (uiFont (9.0f));
    g.drawText ("INPUT",  Rectangle<float> (14.0f, 42.0f, 52.0f, 12.0f), Justification::centred);
    g.drawText ("OUTPUT", Rectangle<float> (354.0f, 42.0f, 52.0f, 12.0f), Justification::centred);
    g.drawText ("PICKUP", Rectangle<float> (14.0f, 64.0f, 46.0f, 24.0f), Justification::centredLeft);
    g.drawText ("VOLUME", Rectangle<float> (336.0f, 64.0f, 44.0f, 24.0f), Justification::centredRight);
}

//==============================================================================
void GristleEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.drawImageAt (background, 0, 0);
    g.addTransform (AffineTransform::scale (scale));

    // display MIDI
    const Rectangle<float> disp (76.0f, 8.0f, 216.0f, 44.0f);
    g.setColour (Colour (0xff0a110b));
    g.fillRoundedRectangle (disp, 4.0f);
    g.setColour (Colour (0xff23262a));
    g.drawRoundedRectangle (disp, 4.0f, 1.0f);

    g.setColour (Colour (0xff9be08a));
    g.setFont (uiFont (10.0f));
    String line1 = "MIDI", line2;
    const int learn = proc.getLearningParam();
    if (learn >= 0)
    {
        line1 = String ("LEARN: ") + displayNames[learn];
        line2 = "muovi un controllo sulla tastiera";
    }
    else if (const int cc = proc.lastCC.load(); cc >= 0)
    {
        int idx = -1;
        for (int i = 0; i < GristleProcessor::numLearnable; ++i)
            if (proc.getCCForParam (i) == cc) { idx = i; break; }
        line2 = "CC " + String (cc) + " = " + String (proc.lastCCValue.load())
                + (idx >= 0 ? String ("  -> ") + displayNames[idx] : String ("  (non assegnato)"));
    }
    else
        line2 = "clic destro su un comando: Learn";
    g.drawText (line1, disp.reduced (10.0f, 5.0f).removeFromTop (16.0f), Justification::centredLeft);
    g.setFont (uiFont (10.0f, false));
    g.setColour (Colour (0xff8fcf80));
    g.drawText (line2, disp.reduced (10.0f, 5.0f).removeFromBottom (16.0f), Justification::centredLeft);
}

void GristleEditor::paintOverChildren (juce::Graphics& g)
{
    const int learn = proc.getLearningParam();
    if (auto* c = controlFor (learn))
    {
        const float a = 0.45f + 0.45f * std::sin (pulse);
        g.setColour (learnCol.withAlpha (a));
        g.drawRoundedRectangle (getLocalArea (c, c->getLocalBounds()).toFloat().expanded (3.0f * scale), 8.0f * scale, 2.0f * scale);
    }
}

juce::Component* GristleEditor::controlFor (int idx)
{
    switch (idx)
    {
        case GristleProcessor::idxSpeed:  return &speed;
        case GristleProcessor::idxDepth:  return &depth;
        case GristleProcessor::idxWave:   return &wave;
        case GristleProcessor::idxMode:   return &modeSw;
        case GristleProcessor::idxBypass: return &bypassSw;
        case GristleProcessor::idxInput:  return &inTrim;
        case GristleProcessor::idxOutput: return &outTrim;
        case GristleProcessor::idxGtrVol: return &gtrVol;
        default:
            return interior != nullptr && interior->isVisible() ? interior->controlFor (idx) : nullptr;
    }
}

void GristleEditor::timerCallback()
{
    const int learn = proc.getLearningParam();
    const int mv = proc.getMapVersion();
    const int cnt = proc.ccCounter.load();

    if (learn >= 0) pulse += 0.35f;
    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    syncPickupBox();
    if (pu != lastPickup)
    {
        lastPickup = pu;
        const bool custom = pu == PickupChoice::custom;
        pickR.setVisible (custom); pickL.setVisible (custom);
        const bool hasPots = pu != PickupChoice::ideal && pu != PickupChoice::line;
        gtrVol.setEnabled (hasPots);
        gtrVol.setAlpha (hasPots ? 1.0f : 0.35f);
    }
    // le scritte del pannello seguono S1, S2 e S3
    const bool env = proc.apvts.getRawParameterValue (ParamIDs::mode)->load() > 0.5f;
    const bool out = proc.apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    const int w = (int) proc.apvts.getRawParameterValue (ParamIDs::wave)->load();
    if (w != lastWave || env != lastEnv || out != lastOut)
    {
        lastWave = w; lastEnv = env; lastOut = out;
        renderBackground();
        repaint();
    }
    if (learn >= 0 || learn != lastLearn || mv != lastMapVersion || cnt != lastCCCount)
    {
        lastLearn = learn; lastMapVersion = mv; lastCCCount = cnt;
        repaint();
    }
}

void GristleEditor::showLearnMenu (int idx, juce::Component* target)
{
    if (idx < 0) return;
    juce::PopupMenu m;
    const int cc = proc.getCCForParam (idx);
    const bool learning = proc.getLearningParam() == idx;

    m.addSectionHeader (displayNames[idx]);
    m.addItem (1, learning ? "Annulla MIDI Learn" : "MIDI Learn (poi muovi un controllo)");
    m.addItem (2, cc >= 0 ? "Rimuovi assegnazione CC " + juce::String (cc) : "Nessun CC assegnato", cc >= 0);
    m.addSeparator();
    m.addItem (3, "Ripristina mappatura predefinita (KeyLab)");

    juce::Component::SafePointer<GristleEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                     [safe, idx, learning] (int r)
                     {
                         if (safe == nullptr) return;
                         auto& p = safe->proc;
                         if (r == 1) { if (learning) p.cancelLearn(); else p.startLearn (idx); }
                         else if (r == 2) p.clearMappingForParam (idx);
                         else if (r == 3) p.resetDefaultMappings();
                         safe->repaint();
                     });
}

//==============================================================================
// Pick-up: ID del menu 1-6 = scelte fisse (6 = Personalizzato), 100+ = chitarre salvate
namespace { const juce::Identifier pickupNameProp ("pickupName"); }

float GristleEditor::currentR() const
{
    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    if (pu == PickupChoice::custom) return proc.apvts.getRawParameterValue (ParamIDs::pickupR)->load();
    return (float) (GristleProcessor::pickupFor (pu, 0, 0).R / 1000.0);
}

float GristleEditor::currentL() const
{
    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    if (pu == PickupChoice::custom) return proc.apvts.getRawParameterValue (ParamIDs::pickupL)->load();
    const auto p = GristleProcessor::pickupFor (pu, 0, 0);
    return p.ideal_ ? 0.0f : (float) p.L;
}

void GristleEditor::setParam (const char* id, float realValue)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void GristleEditor::rebuildPickupBox()
{
    pickupBox.clear (juce::dontSendNotification);
    pickupBox.addItem (juce::String::fromUTF8 ("Ideale (10 k\xce\xa9)"), 1);
    pickupBox.addItem (juce::String::fromUTF8 ("Single coil (6 k\xce\xa9)"), 2);
    pickupBox.addItem (juce::String::fromUTF8 ("P-90 (8 k\xce\xa9)"), 3);
    pickupBox.addItem (juce::String::fromUTF8 ("Humbucker (10 k\xce\xa9)"), 4);
    pickupBox.addItem ("Linea / synth", 5);
    pickupBox.addItem ("Personalizzato", 6);

    const auto& entries = library.getEntries();
    if (! entries.empty())
    {
        pickupBox.addSectionHeading ("Le mie chitarre");
        for (size_t i = 0; i < entries.size(); ++i)
            pickupBox.addItem (entries[i].name, 100 + (int) i);
    }
    lastLibVersion = library.getVersion();
    syncPickupBox();
}

void GristleEditor::syncPickupBox()
{
    if (library.getVersion() != lastLibVersion) { rebuildPickupBox(); return; }

    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    int id = juce::jlimit (1, (int) PickupChoice::count, pu + 1);
    if (pu == PickupChoice::custom)
    {
        const auto name = proc.apvts.state.getProperty (pickupNameProp).toString();
        const int idx = library.indexOf (name);
        if (idx >= 0)
        {
            const auto& e = library.getEntries()[(size_t) idx];
            if (std::abs (e.rKohm - currentR()) < 0.051f && std::abs (e.lHenry - currentL()) < 0.026f)
                id = 100 + idx;
        }
    }
    if (pickupBox.getSelectedId() != id) pickupBox.setSelectedId (id, juce::dontSendNotification);
}

void GristleEditor::pickupChosen()
{
    const int id = pickupBox.getSelectedId();
    if (id >= 1 && id <= PickupChoice::count)
    {
        proc.apvts.state.setProperty (pickupNameProp, juce::String(), nullptr);
        setParam (ParamIDs::pickup, (float) (id - 1));
    }
    else if (id >= 100)
    {
        const auto& entries = library.getEntries();
        const auto i = (size_t) (id - 100);
        if (i >= entries.size()) return;
        setParam (ParamIDs::pickupR, entries[i].rKohm);
        setParam (ParamIDs::pickupL, entries[i].lHenry);
        setParam (ParamIDs::pickup, (float) PickupChoice::custom);
        proc.apvts.state.setProperty (pickupNameProp, entries[i].name, nullptr);
    }
    syncPickupBox();
}

void GristleEditor::showPickupMenu()
{
    library.reload();
    juce::PopupMenu m, del;
    m.addItem (1, "Salva i valori attuali come chitarra...");
    const auto& entries = library.getEntries();
    for (size_t i = 0; i < entries.size(); ++i) del.addItem (1000 + (int) i, entries[i].name);
    m.addSubMenu ("Elimina chitarra", del, ! entries.empty());

    juce::Component::SafePointer<GristleEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pickupMenuBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) { safe->askNameAndSave(); return; }
        const auto& e = safe->library.getEntries();
        const auto i = (size_t) (r - 1000);
        if (i < e.size()) safe->library.remove (e[i].name);
        safe->rebuildPickupBox();
    });
}

void GristleEditor::askNameAndSave()
{
    const float r = currentR(), l = currentL();
    const auto current = proc.apvts.state.getProperty (pickupNameProp).toString();

    nameDialog = std::make_unique<juce::AlertWindow> (
        "Salva pick-up",
        juce::String::fromUTF8 ("Valori attuali: ") + juce::String (r, 1) + juce::String::fromUTF8 (" k\xce\xa9, ")
            + juce::String (l, 2) + juce::String::fromUTF8 (" H.\nNome della chitarra (se esiste gi\xc3\xa0, viene aggiornata):"),
        juce::MessageBoxIconType::NoIcon, this);
    nameDialog->addTextEditor ("name", current, "Nome");
    nameDialog->addButton ("Salva", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog->addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<GristleEditor> safe (this);
    nameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe, r, l] (int result)
    {
        if (safe == nullptr) return;
        const auto name = safe->nameDialog != nullptr ? safe->nameDialog->getTextEditorContents ("name").trim() : juce::String();
        if (result == 1 && name.isNotEmpty())
        {
            safe->library.save (name, r, l);
            safe->setParam (ParamIDs::pickupR, r);
            safe->setParam (ParamIDs::pickupL, l);
            safe->setParam (ParamIDs::pickup, (float) PickupChoice::custom);
            safe->proc.apvts.state.setProperty (pickupNameProp, name, nullptr);
            safe->rebuildPickupBox();
        }
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->nameDialog.reset(); });
    }), false);
}
