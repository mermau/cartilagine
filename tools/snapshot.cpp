// Genera resources/icon.png e gli screenshot dell'interfaccia (uso: crt_snapshot <cartella>)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "BinaryData.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    auto save = [&] (const juce::Image& img, const char* name)
    {
        juce::File f = dir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    };
    save (GristleEditor::createAppIcon (1024), "icon.png");

    GristleProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    // un po' di audio, così l'oscilloscopio e le misure del pannello interno hanno dati
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer none;
    double ph = 0;
    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < 512; ++i) { const float s = 0.2f * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * 196.0 / 48000.0; buf.setSample (0, i, s); buf.setSample (1, i, s); }
        proc.processBlock (buf, none);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (GristleEditor::baseW * 2, GristleEditor::baseH * 2);
    save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), "screenshot.png");
    // per il manuale: solo il pedale, a 3x
    ed->setSize (GristleEditor::baseW * 3, GristleEditor::baseH * 3);
    save (ed->createComponentSnapshot ({ 0, 96 * 3, GristleEditor::baseW * 3, (GristleEditor::baseH - 96) * 3 }, true, 1.0f), "pedale_3x.png");
    ed->setSize (GristleEditor::baseW * 2, GristleEditor::baseH * 2);

    for (auto* c : ed->getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == "INTERNO")
            { b->setToggleState (true, juce::dontSendNotification); b->onClick(); }
    save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), "screenshot_interno.png");
    ed.reset();

    proc.apvts.getParameter (ParamIDs::mode)->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter (ParamIDs::wave)->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter (ParamIDs::bypass)->setValueNotifyingHost (1.0f);
    ed.reset (proc.createEditor());
    ed->setSize (GristleEditor::baseW * 2, GristleEditor::baseH * 2);
    save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), "screenshot_envelope_out.png");
    ed.reset();
    return 0;
}
