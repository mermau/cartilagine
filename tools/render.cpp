// Fa passare un file WAV nel plugin (come in una DAW) e ne scrive il risultato.
// Uso: crt_render ingresso.wav uscita.wav [id=valore ...]   (valori reali dei parametri)
// Esempio: crt_render di.wav out.wav mode=1 depth=1 speed=0.3 wave=0
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::printf ("uso: crt_render in.wav out.wav [id=valore ...]\n"); return 1; }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (juce::String (argv[1]))));
    if (rd == nullptr) { std::printf ("non riesco a leggere %s\n", argv[1]); return 1; }
    const int n = (int) rd->lengthInSamples;
    const double sr = rd->sampleRate;
    juce::AudioBuffer<float> in (2, n);
    rd->read (&in, 0, n, 0, true, true);
    if (rd->numChannels == 1) in.copyFrom (1, 0, in, 0, 0, n);

    GristleProcessor p;
    for (int a = 3; a < argc; ++a)
    {
        const auto s = juce::String (argv[a]);
        if (auto* prm = p.apvts.getParameter (s.upToFirstOccurrenceOf ("=", false, false)))
            prm->setValueNotifyingHost (prm->convertTo0to1 (s.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        else { std::printf ("parametro sconosciuto: %s\n", argv[a]); return 1; }
    }
    const int bs = 512;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    juce::AudioBuffer<float> out (2, n), blk (2, bs);
    juce::MidiBuffer none;
    for (int pos = 0; pos < n; pos += bs)
    {
        const int len = juce::jmin (bs, n - pos);
        blk.setSize (2, len, false, false, true);
        for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, pos, len);
        p.processBlock (blk, none);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, blk, ch, 0, len);
    }
    auto db = [] (double v) { return 20.0 * std::log10 (std::max (v, 1e-9)); };
    std::printf ("ingresso: picco %.1f dBFS, RMS %.1f dBFS | uscita: picco %.1f dBFS, RMS %.1f dBFS\n",
                 db (in.getMagnitude (0, 0, n)), db (in.getRMSLevel (0, 0, n)),
                 db (out.getMagnitude (0, 0, n)), db (out.getRMSLevel (0, 0, n)));

    juce::File f { juce::String (argv[2]) };
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> wr (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    if (wr != nullptr) wr->writeFromAudioSampleBuffer (out, 0, n);
    return 0;
}
