#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PickupLibrary.h"

//==============================================================================
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return juce::Font (juce::FontOptions (juce::jmin (30.0f, (float) b.getHeight() * 0.46f))); }
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.34f), juce::Font::bold));
    }
    // manopola del pedale: nera zigrinata con la linea bianca
    static void drawPedalKnob (juce::Graphics&, juce::Point<float> c, float k, float angle);
};

//==============================================================================
// Slider con menu MIDI Learn sul clic destro
class LearnableSlider : public juce::Slider
{
public:
    std::function<void()> onLearnMenu;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag (e); }
    void mouseUp   (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseUp (e); }
};

// Interruttore a pedale (stomp cromato). Lo stato del pulsante è il parametro collegato.
class FootSwitch : public juce::Button
{
public:
    FootSwitch() : juce::Button ("FootSwitch") { setClickingTogglesState (true); }
    std::function<void()> onLearnMenu;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Button::mouseUp (e); }
    static void drawStomp (juce::Graphics&, juce::Rectangle<float> b, bool down, bool highlighted);
};

// Deviatore a levetta VCA / VCF (stato = parametro "mode": true = VCA in alto)
class ToggleSwitch : public juce::Button
{
public:
    ToggleSwitch() : juce::Button ("Toggle") { setClickingTogglesState (true); }
    std::function<void()> onLearnMenu;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Button::mouseUp (e); }
    static void drawToggle (juce::Graphics&, juce::Rectangle<float> b, bool up, bool highlighted);
};

//==============================================================================
// Pannello "interno" (come aprire la scatola): i quattro trimmer della taratura, con un
// piccolo oscilloscopio sulla forma d'onda e la misura della spazzata del filtro,
// le batterie e la dispersione del JFET.
class InteriorPanel : public juce::Component, private juce::Timer
{
public:
    InteriorPanel (GristleProcessor&, juce::Typeface::Ptr panelFace,
                   std::function<void (int, juce::Component*)> learnMenu);
    ~InteriorPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Component* controlFor (int paramIndex);
    std::function<void()> onClose;

    static constexpr float baseW = 380.0f, baseH = 656.0f;

private:
    void timerCallback() override;
    juce::Font font (float pt, float tracking = 0.0f) const;
    juce::Rectangle<int> area (float x, float y, float w, float h) const;
    void resetToOriginal();

    GristleProcessor& proc;
    juce::Typeface::Ptr face;
    LearnableSlider vr2, vr3, battery, vp;
    juce::TextButton resetBtn { "TARATURA ORIGINALE" }, closeBtn { "CHIUDI" }, noiseBtn { "RUMORE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> noiseA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vr2A, vr3A, batA, vpA;
    float k = 1.0f;
    std::vector<float> trace;
};

//==============================================================================
class GristleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GristleEditor (GristleProcessor&);
    ~GristleEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // icona dell'applicazione Standalone (resources/icon.png)
    static juce::Image createAppIcon (int size);
    // disegno del pedale (senza manopole e interruttori), coordinate di progetto
    static void drawPedal (juce::Graphics&, juce::Rectangle<float> body, juce::Typeface::Ptr panelFace,
                           juce::Typeface::Ptr scriptFace, bool withExtras, int wave);
    // i due LED (velocità e IN), disegnati sopra lo sfondo
    static void drawLed (juce::Graphics&, juce::Point<float> c, float r, float level);

    // coordinate di progetto: barra di servizio 0-96, pedale sotto
    static constexpr int baseW = 420, baseH = 780;
    static constexpr float pedalY = 96.0f;
    // posizioni nel pedale (330 x 540), disposizione dei Gristleizer autocostruiti
    static constexpr float row1Y = 112.0f;
    static constexpr float biasX = 60.0f, depthX = 134.0f, gainX = 208.0f, levelX = 282.0f;
    static constexpr float speedX = 70.0f,  speedY = 222.0f;
    static constexpr float waveX  = 168.0f, waveY  = 222.0f;
    static constexpr float modeX  = 254.0f, modeY  = 222.0f;
    static constexpr float rateLedX = 34.0f, rateLedY = 266.0f;
    static constexpr float inX    = 116.0f, inY    = 474.0f;
    static constexpr float inLedX = 46.0f,  inLedY = 462.0f;
    static constexpr float knobSize = 86.0f, switchSize = 58.0f;

private:
    void timerCallback() override;
    void showLearnMenu (int paramIndex, juce::Component* target);
    void showMidiDeviceMenu();
    void renderBackground();
    juce::Component* controlFor (int paramIndex);
    juce::Rectangle<float> base (float x, float y, float w, float h) const;

    // pick-up: preset fissi + chitarre salvate dall'utente (come FTG-1)
    void rebuildPickupBox();
    void syncPickupBox();
    void pickupChosen();
    void showPickupMenu();
    void askNameAndSave();
    void setParam (const char* id, float realValue);
    float currentR() const;
    float currentL() const;

    GristleProcessor& proc;
    PedalLookAndFeel lnf;

    LearnableSlider speed, depth, wave, bias, gain, level, inTrim, outTrim, gtrVol;
    ToggleSwitch modeSw;
    FootSwitch bypassSw;
    juce::ComboBox pickupBox;
    juce::Slider pickR, pickL;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> speedA, depthA, waveA, biasA, gainA, levelA, inA, outA, gtrA, pickRA, pickLA;
    std::unique_ptr<BA> modeA, bypassA;
    juce::TextButton pickupMenuBtn { juce::String::fromUTF8 ("\xe2\x80\xa6") };
    juce::TextButton interiorBtn { "INTERNO" };
    std::unique_ptr<InteriorPanel> interior;
    PickupLibrary library;
    std::unique_ptr<juce::AlertWindow> nameDialog;
    int lastLibVersion = -1, lastPickup = -1;

    juce::Typeface::Ptr scriptFace, panelFace, uiFace;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Image background;
    float scale = 1.0f;
    int lastLearn = -2, lastMapVersion = -1, lastCCCount = -1;
    int lastWave = -1;
    float lastRateLed = -1.0f; bool lastInLed = false;
    float pulse = 0.0f;
};
