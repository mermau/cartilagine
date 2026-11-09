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
    // manopola del pedale: cilindro nero con la croce chiara sul cappellotto
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
    LearnableSlider vr2, vr3, vr5, vr6, battery, vp;
    juce::TextButton resetBtn { "TARATURA ORIGINALE" }, closeBtn { "CHIUDI" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vr2A, vr3A, vr5A, vr6A, batA, vpA;
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

    // icona dell'applicazione Standalone (resources/icon.png)
    static juce::Image createAppIcon (int size);
    // disegno del pedale (senza manopole e interruttori), coordinate di progetto
    static void drawPedal (juce::Graphics&, juce::Rectangle<float> body, juce::Typeface::Ptr panelFace,
                           juce::Typeface::Ptr scriptFace, bool withExtras, int wave, bool envelope, bool out);

    // coordinate di progetto: barra di servizio 0-96, pedale sotto
    static constexpr int baseW = 420, baseH = 780;
    static constexpr float pedalY = 96.0f;
    // posizioni nel pedale (330 x 540)
    static constexpr float speedX = 92.0f,  speedY = 212.0f;
    static constexpr float depthX = 238.0f, depthY = 142.0f;
    static constexpr float waveX  = 196.0f, waveY  = 300.0f;
    static constexpr float modeX  = 250.0f, modeY  = 396.0f;
    static constexpr float inX    = 112.0f, inY    = 444.0f;
    static constexpr float knobSize = 112.0f, waveSize = 120.0f, switchSize = 56.0f;

private:
    void timerCallback() override;
    void showLearnMenu (int paramIndex, juce::Component* target);
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

    LearnableSlider speed, depth, wave, inTrim, outTrim, gtrVol;
    FootSwitch modeSw, bypassSw;
    juce::ComboBox pickupBox;
    juce::Slider pickR, pickL;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> speedA, depthA, waveA, inA, outA, gtrA, pickRA, pickLA;
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
    int lastWave = -1; bool lastEnv = false, lastOut = false;
    float pulse = 0.0f;
};
