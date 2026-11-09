#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::array<const char*, GristleProcessor::numLearnable> GristleProcessor::learnableIDs {
    ParamIDs::speed, ParamIDs::depth, ParamIDs::wave, ParamIDs::mode, ParamIDs::bypass,
    ParamIDs::bias, ParamIDs::gain, ParamIDs::level,
    ParamIDs::input, ParamIDs::output, ParamIDs::gtrVol,
    ParamIDs::vr2, ParamIDs::vr3, ParamIDs::battery, ParamIDs::jfetVp
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GristleProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto knob = [] (float v, int) { return String (v * 10.0f, 1); };
    auto trim = [] (const char* id, const char* name)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB"));
    };
    auto trimmer = [] (const char* id, const char* name, double def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.0001f), (float) def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v * 100.0f, 1) + " %"; }));
    };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::speed, 1 }, "Speed",
        NormalisableRange<float> (0.0f, 1.0f), 0.45f, AudioParameterFloatAttributes().withStringFromValueFunction (knob)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::depth, 1 }, "Depth",
        NormalisableRange<float> (0.0f, 1.0f), 0.8f, AudioParameterFloatAttributes().withStringFromValueFunction (knob)));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::wave, 1 }, "Waveform",
        StringArray { "Rising ramp", "Falling ramp", "Triangle", "Square" }, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Filter / Envelope",
        StringArray { "Filter", "Envelope" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Out (Bypass)", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::bias, 1 }, "Bias",
        NormalisableRange<float> (0.0f, 1.0f), (float) grz::PanelMod::biasKnob, AudioParameterFloatAttributes().withStringFromValueFunction (knob)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gain, 1 }, "Gain",
        NormalisableRange<float> (0.0f, 1.0f), (float) grz::PanelMod::gainKnob,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String::fromUTF8 ("\xc3\x97") + String (1.0 + 10.0 * grz::detail::logTaper (v), 1); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::level, 1 }, "Level",
        NormalisableRange<float> (0.0f, 1.0f), (float) grz::PanelMod::levelKnob,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String (Decibels::gainToDecibels ((float) grz::detail::logTaper (v), -80.0f), 1) + " dB"; })));

    layout.add (trim (ParamIDs::input,  "Input Trim"));
    layout.add (trim (ParamIDs::output, "Output Trim"));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::pickup, 1 }, "Pickup",
        StringArray { "Ideale", "Single coil", "P-90", "Humbucker", "Linea", "Personalizzato" }, PickupChoice::single));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pickupR, 1 }, "Pickup R",
        NormalisableRange<float> (2.0f, 20.0f, 0.1f), 8.0f, AudioParameterFloatAttributes().withLabel ("kOhm")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pickupL, 1 }, "Pickup L",
        NormalisableRange<float> (0.5f, 10.0f, 0.05f), 4.0f, AudioParameterFloatAttributes().withLabel ("H")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gtrVol, 1 }, "Guitar Volume",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 10.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    // interno: i quattro trimmer (posizione del cursore), batterie, JFET
    layout.add (trimmer (ParamIDs::vr2, "VR2 Triangle Shaping", grz::Trim::vr2));
    layout.add (trimmer (ParamIDs::vr3, "VR3 Triangle Offset",  grz::Trim::vr3));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::battery, 1 }, "Batteries",
        NormalisableRange<float> ((float) Battery::minV, (float) Battery::fullV, 0.01f), (float) Battery::nominalV,
        AudioParameterFloatAttributes().withLabel ("V").withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::jfetVp, 1 }, "JFET Vp",
        NormalisableRange<float> (-6.0f, -1.0f, 0.01f), -3.0f,
        AudioParameterFloatAttributes().withLabel ("V").withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::noise, 1 }, "Circuit Noise", false));
    return layout;
}

GristleProcessor::GristleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    speedRaw   = apvts.getRawParameterValue (ParamIDs::speed);
    depthRaw   = apvts.getRawParameterValue (ParamIDs::depth);
    waveRaw    = apvts.getRawParameterValue (ParamIDs::wave);
    modeRaw    = apvts.getRawParameterValue (ParamIDs::mode);
    bypassRaw  = apvts.getRawParameterValue (ParamIDs::bypass);
    inRaw      = apvts.getRawParameterValue (ParamIDs::input);
    outRaw     = apvts.getRawParameterValue (ParamIDs::output);
    pickupRaw  = apvts.getRawParameterValue (ParamIDs::pickup);
    pickRRaw   = apvts.getRawParameterValue (ParamIDs::pickupR);
    pickLRaw   = apvts.getRawParameterValue (ParamIDs::pickupL);
    gtrRaw     = apvts.getRawParameterValue (ParamIDs::gtrVol);
    batteryRaw = apvts.getRawParameterValue (ParamIDs::battery);
    vpRaw      = apvts.getRawParameterValue (ParamIDs::jfetVp);
    noiseRaw   = apvts.getRawParameterValue (ParamIDs::noise);
    trimRaw    = { apvts.getRawParameterValue (ParamIDs::vr2), apvts.getRawParameterValue (ParamIDs::vr3),
                   apvts.getRawParameterValue (ParamIDs::bias), apvts.getRawParameterValue (ParamIDs::level),
                   apvts.getRawParameterValue (ParamIDs::gain) };
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::bypass));

    for (int i = 0; i < numLearnable; ++i)
    {
        learnParams[(size_t) i] = apvts.getParameter (learnableIDs[(size_t) i]);
        pendingValue[(size_t) i].store (-1.0f);
    }
    for (auto& s : scope) s.store (0.0f);

    resetDefaultMappings();
    startTimerHz (60);
    autoSelectKeyLab();
}

//==============================================================================
// Ingresso MIDI diretto
void GristleProcessor::autoSelectKeyLab()
{
    // di fabbrica: la porta "MIDI" della KeyLab (non quella "DAW"), se è collegata
    const auto devices = juce::MidiInput::getAvailableDevices();
    juce::String best;
    for (const auto& d : devices)
        if (d.name.containsIgnoreCase ("keylab"))
        {
            if (best.isEmpty() || (d.name.containsIgnoreCase ("midi") && ! d.name.containsIgnoreCase ("daw")))
                best = d.identifier;
        }
    if (best.isNotEmpty()) setMidiDevice (best);
}

void GristleProcessor::setMidiDevice (const juce::String& identifier)
{
    if (midiIn != nullptr) { midiIn->stop(); midiIn.reset(); }
    deviceActive.store (false);
    midiDeviceId = identifier;
    midiDeviceName = {};
    if (identifier.isNotEmpty())
    {
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (d.identifier == identifier) midiDeviceName = d.name;
        midiIn = juce::MidiInput::openDevice (identifier, this);
        if (midiIn != nullptr)
        {
            midiIn->start();
            deviceActive.store (true);
        }
    }
    ++mapVersion;
}

juce::String GristleProcessor::getMidiDeviceName() const
{
    if (midiDeviceId.isEmpty()) return {};
    return midiDeviceName.isNotEmpty() ? midiDeviceName : midiDeviceId;
}

// thread MIDI del sistema: solo atomici, come per i CC dalla DAW
void GristleProcessor::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isController()) handleCC (m.getControllerNumber(), m.getControllerValue());
}

//==============================================================================
void GristleProcessor::resetDefaultMappings()
{
    for (auto& m : ccToParam) m.store (-1);
    // KeyLab mkII in modalità User: encoder 1-6 = CC 74, 71, 76, 77, 93, 18
    ccToParam[74].store (idxSpeed);
    ccToParam[71].store (idxDepth);
    ccToParam[76].store (idxWave);
    ccToParam[77].store (idxBias);      // sposta il centro della spazzata
    ccToParam[93].store (idxGain);
    ccToParam[18].store (idxLevel);
    // pedali: sustain (64) = IN/OUT, CC 65 = FILTER/ENVELOPE (commutano alla pressione)
    ccToParam[64].store (idxBypass);
    ccToParam[65].store (idxMode);
    ++mapVersion;
}

int GristleProcessor::getLearnIndex (const juce::String& paramID) const
{
    for (int i = 0; i < numLearnable; ++i)
        if (paramID == learnableIDs[(size_t) i]) return i;
    return -1;
}

int GristleProcessor::getCCForParam (int paramIndex) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (ccToParam[(size_t) cc].load() == paramIndex) return cc;
    return -1;
}

void GristleProcessor::clearMappingForParam (int paramIndex)
{
    for (auto& m : ccToParam)
        if (m.load() == paramIndex) m.store (-1);
    ++mapVersion;
}

// thread audio: niente chiamate all'host, solo atomici
void GristleProcessor::handleCC (int cc, int value)
{
    lastCC.store (cc); lastCCValue.store (value); ++ccCounter;

    int learn = learningParam.load();
    if (learn >= 0 && learningParam.compare_exchange_strong (learn, -1))
    {
        for (auto& m : ccToParam)
            if (m.load() == learn) m.store (-1);
        ccToParam[(size_t) cc].store (learn);
        ccHigh[(size_t) cc].store (value >= 64);
        ++mapVersion;
        return; // il messaggio usato per l'apprendimento non modifica il parametro
    }

    const int idx = ccToParam[(size_t) cc].load();
    if (idx < 0) return;
    auto& pending = pendingValue[(size_t) idx];

    if (idx == idxBypass || idx == idxMode)   // interruttori a pedale: commutano alla pressione
    {
        const bool high = value >= 64;
        if (high && ! ccHigh[(size_t) cc].exchange (true))
        {
            const float cur = pending.load() >= 0.0f ? pending.load() : learnParams[(size_t) idx]->getValue();
            pending.store (cur > 0.5f ? 0.0f : 1.0f);
        }
        if (! high) ccHigh[(size_t) cc].store (false);
    }
    else
    {
        pending.store ((float) value / 127.0f);
    }
}

// thread dei messaggi: applica i CC ricevuti ai parametri (notifica all'host)
void GristleProcessor::timerCallback()
{
    for (int i = 0; i < numLearnable; ++i)
    {
        const float v = pendingValue[(size_t) i].exchange (-1.0f);
        if (v < 0.0f) continue;
        auto* p = learnParams[(size_t) i];
        if (std::abs (p->getValue() - v) < 1.0e-6f) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }
}

grz::Pickup GristleProcessor::pickupFor (int choice, float rKohm, float lHenry)
{
    switch (choice)
    {
        case PickupChoice::ideal:     return grz::Pickup::ideal();
        case PickupChoice::single:    return grz::Pickup::singleCoil();
        case PickupChoice::p90:       return grz::Pickup::p90();
        case PickupChoice::humbucker: return grz::Pickup::humbucker();
        case PickupChoice::line:      return grz::Pickup::line();
        default:                      return { rKohm * 1000.0, (double) lHenry, 150e-12, 250e3 };
    }
}

// VR2, VR3 (trimmer interni), BIAS (VR5 + 220k), LEVEL (VR6, pot log), GAIN (al posto di R21)
void GristleProcessor::applyTrims (float vr2, float vr3, float bias, float level, float gain)
{
    circuit.setTrimmers (vr2, vr3, bias, grz::detail::logTaper (level));
    circuit.setGain (gain);
}

grz::Pickup GristleProcessor::currentPickup() const
{
    return pickupFor ((int) pickupRaw->load(), pickRRaw->load(), pickLRaw->load());
}

//==============================================================================
bool GristleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void GristleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (size_t) juce::jmax (1, getTotalNumOutputChannels());
    maxBlock = juce::jmax (1, samplesPerBlock);
    osChannels = (int) numCh;
    hostRate = sampleRate;

    // il circuito gira a circa 350-390 kHz: 8x a 44.1/48 kHz, 4x a 88.2/96, 2x a 176.4/192.
    // Sotto questa frequenza il modello di IC5 (GBW e slew rate del 741) non è più affidabile.
    const int order = sampleRate > 300000.0 ? 0 : (sampleRate > 120000.0 ? 1 : (sampleRate > 60000.0 ? 2 : 3));
    using OS = juce::dsp::Oversampling<float>;
    oversampling = std::make_unique<OS> (1, (size_t) order, OS::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    circuit.setPickup (currentPickup());
    circuit.setGuitarVolume (gtrRaw->load() / 10.0f);
    circuit.setSpeed (speedRaw->load());
    circuit.setDepth (depthRaw->load());
    circuit.setWave ((int) waveRaw->load());
    circuit.setFilter (modeRaw->load() < 0.5f);
    circuit.setSupply (batteryRaw->load());
    circuit.setJfetVto (vpRaw->load());
    circuit.setNoise (noiseRaw->load() > 0.5f);
    circuit.setBiasTop (grz::PanelMod::biasTop);
    applyTrims (trimRaw[0]->load(), trimRaw[1]->load(), trimRaw[2]->load(), trimRaw[3]->load(), trimRaw[4]->load());
    circuit.prepare (sampleRate * (double) oversampling->getOversamplingFactor());

    mixer.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    const auto latency = oversampling->getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));
    mixer.setWetLatency (latency);
    mixer.setWetMixProportion (bypassRaw->load() > 0.5f ? 0.0f : 1.0f);

    bypassFadeSamples = juce::roundToInt (0.08 * sampleRate);
    lastBypassState = bypassRaw->load() > 0.5f;
    bypassFadeLeft = 0;
    circuitIdle = lastBypassState;

    auto initSm = [sampleRate] (juce::SmoothedValue<float>& s, float v, double sec)
    { s.reset (sampleRate, sec); s.setCurrentAndTargetValue (v); };
    initSm (speedSm,   speedRaw->load(), 0.03);
    initSm (depthSm,   depthRaw->load(), 0.03);
    initSm (inGainSm,  juce::Decibels::decibelsToGain (inRaw->load()),  0.03);
    initSm (outGainSm, juce::Decibels::decibelsToGain (outRaw->load()), 0.03);
    initSm (vbatSm,    batteryRaw->load(), 0.15);
    initSm (vpSm,      vpRaw->load(), 0.10);
    initSm (gtrSm,     gtrRaw->load() / 10.0f, 0.03);
    for (size_t k = 0; k < trimSm.size(); ++k) initSm (trimSm[k], trimRaw[k]->load(), 0.05);

    sampleClock = 0; lastWrap = -1; phaseArmed = false; gWindow = 0; scopeDiv = 0;
    gMin = gMax = (float) circuit.gate();
    scopeRate.store ((float) (sampleRate / 8.0));
}

void GristleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // con la porta della tastiera aperta direttamente, i CC della DAW si ignorano
    // (altrimenti un pedale arriverebbe due volte e commuterebbe due volte)
    if (! deviceActive.load())
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isController()) handleCC (m.getControllerNumber(), m.getControllerValue());
        }
    midi.clear();

    const int numCh = juce::jmin (getTotalNumInputChannels(), osChannels);
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh < 1) return;

    circuit.setPickup (currentPickup());
    circuit.setNoise (noiseRaw->load() > 0.5f);
    circuit.setWave ((int) waveRaw->load());
    circuit.setFilter (modeRaw->load() < 0.5f);
    speedSm.setTargetValue (speedRaw->load());
    depthSm.setTargetValue (depthRaw->load());
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (inRaw->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (outRaw->load()));
    vbatSm.setTargetValue (batteryRaw->load());
    vpSm.setTargetValue (vpRaw->load());
    gtrSm.setTargetValue (gtrRaw->load() / 10.0f);
    for (size_t k = 0; k < trimSm.size(); ++k) trimSm[k].setTargetValue (trimRaw[k]->load());

    const bool bypassed = bypassRaw->load() > 0.5f;
    mixer.setWetMixProportion (bypassed ? 0.0f : 1.0f);

    // S3 su OUT: dopo la dissolvenza il circuito resta fermo e non consuma CPU;
    // tornando su IN riparte dal punto di riposo (LFO all'inizio della rampa)
    if (bypassed && ! lastBypassState) bypassFadeLeft = bypassFadeSamples;
    if (! bypassed && circuitIdle)
    {
        for (auto* s : { &speedSm, &depthSm, &vbatSm, &vpSm, &gtrSm }) s->setCurrentAndTargetValue (s->getTargetValue());
        for (auto& s : trimSm) s.setCurrentAndTargetValue (s.getTargetValue());
        circuit.setSpeed (speedSm.getCurrentValue());
        circuit.setDepth (depthSm.getCurrentValue());
        circuit.setSupply (vbatSm.getCurrentValue());
        circuit.setJfetVto (vpSm.getCurrentValue());
        circuit.setGuitarVolume (gtrSm.getCurrentValue());
        applyTrims (trimSm[0].getCurrentValue(), trimSm[1].getCurrentValue(), trimSm[2].getCurrentValue(),
                    trimSm[3].getCurrentValue(), trimSm[4].getCurrentValue());
        circuit.reset();
        oversampling->reset();
        circuitIdle = false;
    }
    lastBypassState = bypassed;
    if (bypassed && ! circuitIdle)
    {
        bypassFadeLeft -= buffer.getNumSamples();
        if (bypassFadeLeft < -buffer.getNumSamples()) circuitIdle = true;
    }
    running.store (! circuitIdle);

    juce::dsp::AudioBlock<float> full (buffer);
    full = full.getSubsetChannelBlock (0, (size_t) numCh);
    const int factor = (int) oversampling->getOversamplingFactor();

    for (int offset = 0; offset < buffer.getNumSamples(); offset += maxBlock)
    {
        const int n = juce::jmin (maxBlock, buffer.getNumSamples() - offset);
        auto block = full.getSubBlock ((size_t) offset, (size_t) n);
        auto mono  = block.getSingleChannelBlock (0);

        mixer.pushDrySamples (block);
        if (circuitIdle) { mixer.mixWetSamples (block); continue; }

        auto* x = mono.getChannelPointer (0);

        // la chitarra può arrivare su uno qualsiasi dei due ingressi: se i canali sono
        // copie identiche si usa il primo, altrimenti si sommano
        if (numCh > 1)
        {
            const auto* r = block.getChannelPointer (1);
            bool identical = true;
            for (int i = 0; i < n && identical; ++i) identical = std::abs (x[i] - r[i]) < 1.0e-7f;
            if (! identical) juce::FloatVectorOperations::add (x, r, n);
        }

        for (int i = 0; i < n; ++i) x[i] *= inGainSm.getNextValue() * voltsPerFS;

        auto up = oversampling->processSamplesUp (mono);
        auto* d = up.getChannelPointer (0);

        for (int start = 0; start < n; start += controlBlock)
        {
            const int len = juce::jmin (controlBlock, n - start);
            circuit.setSpeed (speedSm.skip (len));
            circuit.setDepth (depthSm.skip (len));
            circuit.setSupply (vbatSm.skip (len));
            circuit.setJfetVto (vpSm.skip (len));
            circuit.setGuitarVolume (gtrSm.skip (len));
            const float t0 = trimSm[0].skip (len), t1 = trimSm[1].skip (len), t2 = trimSm[2].skip (len),
                        t3 = trimSm[3].skip (len), t4 = trimSm[4].skip (len);
            applyTrims (t0, t1, t2, t3, t4);

            for (int j = 0; j < len; ++j)
            {
                const int base = (start + j) * factor;
                for (int k = 0; k < factor; ++k)
                    d[base + k] = (float) circuit.process ((double) d[base + k]);

                // misure per il pannello interno (a 1/8 della frequenza dell'host)
                ++sampleClock;
                if (++scopeDiv >= 8)
                {
                    scopeDiv = 0;
                    const int w = scopeWrite.load (std::memory_order_relaxed);
                    scope[(size_t) w].store ((float) circuit.waveOut(), std::memory_order_relaxed);
                    scopeWrite.store ((w + 1) % scopeSize, std::memory_order_release);

                    const double ph = circuit.lfoPhase();
                    if (ph > 0.9) phaseArmed = true;
                    if (phaseArmed && ph < 0.3)
                    {
                        phaseArmed = false;
                        if (lastWrap >= 0)
                        {
                            const float hz = (float) (hostRate / (double) (sampleClock - lastWrap));
                            const float old = lfoHz.load();
                            lfoHz.store (old > 0.0f ? old + 0.5f * (hz - old) : hz);
                        }
                        lastWrap = sampleClock;
                    }
                    // LFO quasi fermo: la frequenza misurata decade
                    if (lastWrap >= 0 && sampleClock - lastWrap > (long long) (hostRate * 4.0))
                    {
                        const float bound = (float) (hostRate / (double) (sampleClock - lastWrap));
                        if (lfoHz.load() > bound) lfoHz.store (bound);
                    }

                    ledLevel.store ((float) circuit.ledDrive(), std::memory_order_relaxed);
                    const float g = (float) circuit.gate();
                    gMin = std::min (gMin, g); gMax = std::max (gMax, g);
                    if (++gWindow >= (int) (hostRate / 8.0 * 1.5))   // finestra di 1,5 s
                    {
                        gateMin.store (gMin); gateMax.store (gMax);
                        gMin = gMax = g; gWindow = 0;
                    }
                }
            }
        }

        oversampling->processSamplesDown (mono);

        for (int i = 0; i < n; ++i) x[i] *= outGainSm.getNextValue() * fsPerVolt;
        for (int ch = 1; ch < numCh; ++ch)
            juce::FloatVectorOperations::copy (block.getChannelPointer ((size_t) ch), x, n);

        mixer.mixWetSamples (block);
    }
}

//==============================================================================
void GristleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("CartilagineState");
    if (auto params = apvts.copyState().createXml()) root.addChildElement (params.release());

    auto* dev = root.createNewChildElement ("MIDIDEVICE");
    dev->setAttribute ("id", midiDeviceId);
    dev->setAttribute ("name", midiDeviceName);

    auto* map = root.createNewChildElement ("MIDIMAP");
    for (int cc = 0; cc < 128; ++cc)
        if (const int idx = ccToParam[(size_t) cc].load(); idx >= 0)
        {
            auto* e = map->createNewChildElement ("CC");
            e->setAttribute ("num", cc);
            e->setAttribute ("param", learnableIDs[(size_t) idx]);
        }
    copyXmlToBinary (root, destData);
}

void GristleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("CartilagineState")) return;

    if (auto* params = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    if (auto* dev = xml->getChildByName ("MIDIDEVICE"))
    {
        auto id = dev->getStringAttribute ("id");
        const auto name = dev->getStringAttribute ("name");
        // l'identificatore può cambiare tra un computer e l'altro: si cerca anche per nome
        bool found = id.isEmpty();
        for (const auto& d : juce::MidiInput::getAvailableDevices())
        {
            if (d.identifier == id) { found = true; break; }
            if (name.isNotEmpty() && d.name == name) { id = d.identifier; found = true; break; }
        }
        juce::MessageManager::callAsync ([id, found, name, safe = juce::WeakReference<GristleProcessor> (this)]
        {
            if (safe == nullptr) return;
            safe->setMidiDevice (found ? id : juce::String());     // non collegata: CC dalla DAW
        });
    }

    if (auto* map = xml->getChildByName ("MIDIMAP"))
    {
        for (auto& m : ccToParam) m.store (-1);
        for (auto* e : map->getChildWithTagNameIterator ("CC"))
        {
            const int cc = e->getIntAttribute ("num", -1);
            const int idx = getLearnIndex (e->getStringAttribute ("param"));
            if (juce::isPositiveAndBelow (cc, 128) && idx >= 0) ccToParam[(size_t) cc].store (idx);
        }
        ++mapVersion;
    }
}

juce::AudioProcessorEditor* GristleProcessor::createEditor() { return new GristleEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GristleProcessor(); }
