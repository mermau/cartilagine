// Test del processore come lo userebbe una DAW: blocchi stereo a 48 kHz, CC MIDI,
// interruttori a pedale, MIDI Learn, stato salvato/ricaricato, CPU.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include <chrono>

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    bool ok = true;
    auto check = [&] (bool c, const char* what) { std::printf ("%-60s %s\n", what, c ? "OK" : "ERRORE"); ok &= c; };
    auto value = [] (GristleProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };

    GristleProcessor p;
    const double sr = 48000; const int bs = 512;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    std::printf ("latenza: %d campioni\n", p.getLatencySamples());

    juce::AudioBuffer<float> buf (2, bs);
    double phase = 0;
    auto fill = [&] (float amp)
    {
        for (int i = 0; i < bs; ++i)
        {
            // dente di sega a 196 Hz (ricco di armoniche, come una corda pizzicata)
            const float s = amp * (float) (2.0 * (phase - std::floor (phase)) - 1.0); phase += 196.0 / sr;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
    };
    auto rms = [&] { return buf.getRMSLevel (0, 0, bs); };
    juce::MidiBuffer none;

    // suono a regime
    float rIn = 0, rOut = 0; bool finite = true;
    for (int b = 0; b < 200; ++b) { fill (0.2f); rIn = rms(); p.processBlock (buf, none); rOut = rms(); finite &= std::isfinite (rOut); }
    std::printf ("ingresso %.3f rms, uscita %.3f rms (FILTER, triangolo)\n", rIn, rOut);
    check (finite && rOut > 0.005f, "audio elaborato, nessun NaN");
    check (rOut < 10.0f * rIn && rOut > 0.1f * rIn, "LEVEL predefinito (-6 dB): livello d'uscita ragionevole");

    // CC 74 -> SPEED, CC 71 -> DEPTH, CC 76 -> forma d'onda
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::controllerEvent (1, 74, 0), 0);
    m.addEvent (juce::MidiMessage::controllerEvent (1, 71, 127), 1);
    m.addEvent (juce::MidiMessage::controllerEvent (1, 76, 127), 2);
    fill (0.2f); p.processBlock (buf, m); pump (100);
    check (value (p, ParamIDs::speed) < 0.01f, "CC 74 = 0 porta SPEED a 0");
    check (value (p, ParamIDs::depth) > 0.99f, "CC 71 = 127 porta DEPTH al massimo");
    check ((int) value (p, ParamIDs::wave) == 3, "CC 76 = 127 sceglie la quadra");

    // CC 65 premuto -> ENVELOPE; rilascio: niente; nuova pressione -> FILTER
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 65, 127), 0);
    fill (0.2f); p.processBlock (buf, m); pump (100);
    check (value (p, ParamIDs::mode) > 0.5f, "CC 65 commuta su ENVELOPE");
    for (int b = 0; b < 40; ++b) { fill (0.2f); p.processBlock (buf, none); finite &= std::isfinite (rms()); }
    check (finite, "ENVELOPE: nessun NaN");
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 65, 0), 0);
    m.addEvent (juce::MidiMessage::controllerEvent (1, 65, 127), 10);
    fill (0.2f); p.processBlock (buf, m); pump (100);
    check (value (p, ParamIDs::mode) < 0.5f, "rilascio e nuova pressione: torna su FILTER");

    // CC 64 premuto -> OUT (bypass): segnale diretto
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    fill (0.2f); p.processBlock (buf, m); pump (100);
    check (value (p, ParamIDs::bypass) > 0.5f, "CC 64 commuta su OUT");
    for (int b = 0; b < 40; ++b) { fill (0.2f); p.processBlock (buf, none); }
    {
        juce::AudioBuffer<float> ref (2, bs);
        fill (0.2f); ref.makeCopyOf (buf); p.processBlock (buf, none);
        float diff = 0;
        const int L = p.getLatencySamples();
        for (int i = L; i < bs; ++i) diff = std::max (diff, std::abs (buf.getSample (0, i) - ref.getSample (0, i - L)));
        check (diff < 1e-4f, "OUT = segnale diretto (come S3 dell'originale)");
    }
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 10);
    fill (0.2f); p.processBlock (buf, m); pump (100);
    check (value (p, ParamIDs::bypass) < 0.5f, "nuova pressione: di nuovo IN");
    for (int b = 0; b < 40; ++b) { fill (0.2f); p.processBlock (buf, none); finite &= std::isfinite (rms()); }
    check (finite && rms() > 0.001f, "IN: il circuito riparte");

    // MIDI Learn: CC 20 su BIAS
    p.startLearn (GristleProcessor::idxBias);
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 20, 64), 0);
    fill (0.2f); p.processBlock (buf, m); pump (50);
    check (p.getCCForParam (GristleProcessor::idxBias) == 20 && p.getLearningParam() < 0, "MIDI Learn: CC 20 assegnato a BIAS");
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 20, 0), 0);
    fill (0.2f); p.processBlock (buf, m); pump (100);
    check (value (p, ParamIDs::bias) < 0.01f, "CC 20 muove BIAS");

    // stato salvato e ricaricato
    juce::MemoryBlock state; p.getStateInformation (state);
    GristleProcessor q;
    q.setStateInformation (state.getData(), (int) state.getSize());
    check (q.getCCForParam (GristleProcessor::idxBias) == 20 && q.getCCForParam (GristleProcessor::idxSpeed) == 74,
           "stato: mappa MIDI e parametri ricaricati");
    check (std::abs (value (q, ParamIDs::bias) - value (p, ParamIDs::bias)) < 1e-6f && (int) value (q, ParamIDs::wave) == 3,
           "stato: trimmer e forma d'onda ricaricati");

    // CPU a 48 kHz
    {
        p.apvts.getParameter (ParamIDs::bias)->setValueNotifyingHost (p.apvts.getParameter (ParamIDs::bias)->convertTo0to1 ((float) grz::PanelMod::biasKnob));
        pump (50);
        const int blocks = (int) (sr * 4 / bs);
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) { fill (0.2f); p.processBlock (buf, none); }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("CPU: %.1f%% di un core (48 kHz, blocchi da %d)\n", 100.0 * sec / 4.0, bs);
    }
    std::printf ("LFO misurato: %.2f Hz, gate da %.2f a %.2f V\n", p.lfoHz.load(), p.gateMin.load(), p.gateMax.load());
    check (p.lfoHz.load() > 0.0f, "misura della frequenza dell'LFO per il pannello interno");
    return ok ? 0 : 1;
}
