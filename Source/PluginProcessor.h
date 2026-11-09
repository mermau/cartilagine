#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "GristleCircuit.h"

namespace ParamIDs
{
    // comandi del pedale
    inline constexpr const char* speed   = "speed";     // VR1
    inline constexpr const char* depth   = "depth";     // VR4
    inline constexpr const char* wave    = "wave";      // S1
    inline constexpr const char* mode    = "mode";      // S2: 0 = FILTER, 1 = ENVELOPE
    inline constexpr const char* bypass  = "bypass";    // S3: true = OUT
    // barra di servizio
    inline constexpr const char* input   = "input";
    inline constexpr const char* output  = "output";
    inline constexpr const char* pickup  = "pickup";
    inline constexpr const char* pickupR = "pickupR";
    inline constexpr const char* pickupL = "pickupL";
    inline constexpr const char* gtrVol  = "gtrVol";
    inline constexpr const char* bias    = "bias";      // VR5 portato sul pannello (con 220k verso +9 V)
    inline constexpr const char* gain    = "gain";      // pot al posto di R21 (guadagno di IC4)
    inline constexpr const char* level   = "level";     // VR6 portato sul pannello (log)
    // interno
    inline constexpr const char* vr2     = "vr2";       // TRIANGLE SHAPING
    inline constexpr const char* vr3     = "vr3";       // TRIANGLE OFFSET
    inline constexpr const char* battery = "battery";   // tensione delle due PP3
    inline constexpr const char* jfetVp  = "jfetVp";    // Vgs(off) del 2N3819
}

namespace PickupChoice { enum { ideal = 0, single, p90, humbucker, line, custom, count }; }

namespace Battery
{
    inline constexpr double fullV = 9.6, minV = 6.0, nominalV = 9.0;
    // carica residua (0..1) di una 9 V alcalina in funzione della tensione
    inline double chargeFromVolts (double v)
    {
        static const double V[] = { 5.4, 6.6, 7.4, 8.1, 8.7, 9.1, 9.6 };
        static const double C[] = { 0.0, 0.10, 0.25, 0.50, 0.75, 0.90, 1.0 };
        if (v <= V[0]) return 0.0;
        for (int i = 0; i < 6; ++i)
            if (v <= V[i + 1]) return C[i] + (v - V[i]) / (V[i + 1] - V[i]) * (C[i + 1] - C[i]);
        return 1.0;
    }
}

//==============================================================================
class GristleProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    GristleProcessor();
    ~GristleProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // MIDI CC: mappa e MIDI Learn (senza lock). I CC arrivano sul thread audio, che
    // scrive in una coda di atomici; un timer sul thread dei messaggi li applica ai
    // parametri (così l'host riceve le notifiche dal thread giusto).
    static constexpr int numLearnable = 15;
    static const std::array<const char*, numLearnable> learnableIDs;
    static constexpr int idxSpeed = 0, idxDepth = 1, idxWave = 2, idxMode = 3, idxBypass = 4,
                         idxBias = 5, idxGain = 6, idxLevel = 7,
                         idxInput = 8, idxOutput = 9, idxGtrVol = 10,
                         idxVr2 = 11, idxVr3 = 12, idxBattery = 13, idxVp = 14;

    int  getLearnIndex (const juce::String& paramID) const;
    void startLearn (int paramIndex)  { learningParam.store (paramIndex); }
    void cancelLearn()                { learningParam.store (-1); }
    int  getLearningParam() const     { return learningParam.load(); }
    int  getCCForParam (int paramIndex) const;
    void clearMappingForParam (int paramIndex);
    void resetDefaultMappings();
    int  getMapVersion() const        { return mapVersion.load(); }

    std::atomic<int> lastCC { -1 }, lastCCValue { 0 }, ccCounter { 0 };

    static grz::Pickup pickupFor (int choice, float rKohm, float lHenry);

    //==========================================================================
    // Strumenti per la taratura (pannello interno): "oscilloscopio" sull'uscita della
    // forma d'onda (cima di VR4), frequenza misurata dell'LFO, escursione del gate.
    static constexpr int scopeSize = 16384;              // campioni ogni 8 della frequenza dell'host
    std::array<std::atomic<float>, scopeSize> scope;
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> lfoHz { 0.0f }, gateMin { 0.0f }, gateMax { 0.0f }, scopeRate { 6000.0f };
    std::atomic<float> ledLevel { 0.0f };                 // tensione della forma d'onda per il LED
    std::atomic<bool> running { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleCC (int cc, int value);
    void timerCallback() override;

    grz::GristleCircuit circuit;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    double hostRate = 48000.0;
    juce::dsp::DryWetMixer<float> mixer { 256 };

    juce::SmoothedValue<float> speedSm, depthSm, inGainSm, outGainSm, vbatSm, vpSm, gtrSm;
    std::array<juce::SmoothedValue<float>, 5> trimSm;   // VR2, VR3, BIAS, LEVEL, GAIN

    std::atomic<float>* speedRaw   = nullptr;
    std::atomic<float>* depthRaw   = nullptr;
    std::atomic<float>* waveRaw    = nullptr;
    std::atomic<float>* modeRaw    = nullptr;
    std::atomic<float>* bypassRaw  = nullptr;
    std::atomic<float>* inRaw      = nullptr;
    std::atomic<float>* outRaw     = nullptr;
    std::atomic<float>* pickupRaw  = nullptr;
    std::atomic<float>* pickRRaw   = nullptr;
    std::atomic<float>* pickLRaw   = nullptr;
    std::atomic<float>* gtrRaw     = nullptr;
    std::atomic<float>* batteryRaw = nullptr;
    std::atomic<float>* vpRaw      = nullptr;
    std::array<std::atomic<float>*, 5> trimRaw {};
    grz::Pickup currentPickup() const;
    void applyTrims (float vr2, float vr3, float bias, float level, float gain);
    juce::AudioParameterBool* bypassParam = nullptr;

    std::array<juce::RangedAudioParameter*, numLearnable> learnParams {};
    std::array<std::atomic<int>, 128> ccToParam;
    std::array<bool, 128> ccHigh {};
    std::atomic<int> learningParam { -1 }, mapVersion { 0 };
    std::array<std::atomic<float>, numLearnable> pendingValue;

    int maxBlock = 512, osChannels = 2;
    int bypassFadeSamples = 4000, bypassFadeLeft = 0;
    bool lastBypassState = false, circuitIdle = false;

    // misure per il pannello interno
    long long sampleClock = 0, lastWrap = -1;
    bool phaseArmed = false;
    float gMin = 0.0f, gMax = 0.0f;
    int gWindow = 0, scopeDiv = 0;

    static constexpr int controlBlock = 32;       // campioni (frequenza dell'host)
    static constexpr float voltsPerFS = 1.0f;     // 0 dBFS = 1 V di picco all'ingresso
    static constexpr float fsPerVolt  = 1.0f;     // livello d'uscita reale del pedale

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GristleProcessor)
};
