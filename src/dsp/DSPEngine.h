#pragma once

#include "EQBand.h"
#include "Compressor.h"
#include "Limiter.h"
#include "StereoStage.h"
#include "Analyzer.h"
#include <memory>
#include <vector>

class DSPEngine
{
public:
    DSPEngine();
    ~DSPEngine();

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    void setBypass(int module, bool bypassed);
    bool isBypassed(int module) const
    {
        return (module >= 0 && module < 3) ? bypass[module] : true;
    }

    // Modules
    std::vector<std::unique_ptr<EQBand>> eqBands;
    std::unique_ptr<Compressor> compressor;
    std::unique_ptr<Limiter> limiter;
    // Mono, inversione di fase e ampiezza del campo: i tre controlli sotto
    // il vectorscope, che finora non avevano niente da comandare.
    std::unique_ptr<StereoStage> stereo;
    std::unique_ptr<Analyzer> analyzer;

    // Module indices for bypass
    enum Module { EqModule = 0, CompModule = 1, LimitModule = 2 };

private:
    // EQ, compressore e limiter partono bypassati: WhyCremisi si carica sul
    // master e deve essere trasparente finche' non gli si chiede di intervenire.
    // Con i default di fabbrica (comp -24 dB 4:1, limiter -6 dB) il plugin
    // appena inserito attenuava di ~12 dB un segnale a -9 dBFS RMS, senza che
    // nessuno l'avesse chiesto e senza modo di spegnerlo: setBypass() non e'
    // ancora raggiungibile ne' dalla UI ne' dal bridge OSC.
    // L'analyzer resta sempre attivo: serve al metering e non tocca l'audio.
    bool bypass[3] = { true, true, true };
};
