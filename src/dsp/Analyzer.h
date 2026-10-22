#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <deque>
#include <atomic>
#include <memory>
#include "libebur128/ebur128.h"

class Analyzer
{
public:
    struct FFTData {
        std::vector<float> magnitudes;          // per-bin magnitudes (smoothed)
        std::vector<float> rawMagnitudes;       // per-bin raw magnitudes (unsmoothed)
        std::vector<float> phases;              // per-bin phases
        float corrMono = 0.0f;                  // stereo correlation (-1 to 1)
        float momentaryLoudness = 0.0f;         // LUFS momentary (400ms)
        float shortTermLoudness = 0.0f;         // LUFS short-term (3s)
        float integratedLoudness = 0.0f;        // LUFS integrated (full session)
        float truePeak = 0.0f;                  // true peak in dBTP
        float rms = 0.0f;                       // raw RMS in dB
        int clippingCount = 0;                  // clipping event count

        // ── Misure di mix ──────────────────────────────────────────
        //
        //  Le stesse che usiamo fuori dal plugin con analisi_completa.py
        //  per giudicare un pezzo. Averle qui dentro significa che l'AI
        //  puo' leggerle mentre suona, invece di doverle far calcolare a
        //  uno script su un file gia' esportato.

        float loudnessRange = 0.0f;   // LRA in LU: quanto respira il pezzo
        float spectralCentroid = 0.0f; // Hz: dove sta il baricentro del suono
        float spectralRolloff = 0.0f;  // Hz sotto cui sta l'85% dell'energia

        // Energia percentuale per banda, dai bassi agli acuti. Serve a
        // dire "manca il medio-basso" con un numero invece che a orecchio.
        static constexpr int numBands = 10;
        std::array<float, numBands> bandEnergy {};

        // Tempo e tonalita' stimati sul segnale che sta passando.
        // Zero / stringa vuota finche' non c'e' abbastanza materiale.
        float bpm = 0.0f;
        float bpmConfidence = 0.0f;    // 0..1, quanto e' netto il periodo trovato
        juce::String key;              // per esempio "F# min"
        float keyConfidence = 0.0f;    // 0..1
        std::array<float, 12> chroma {}; // energia per classe di altezza, da Do

        // Nuvola di punti per il vectorscope, gia' ruotata di 45 gradi:
        // x = side (L-R), y = mid (L+R), entrambi in -1..1. Interleaved
        // x,y,x,y... Sono i campioni veri decimati, non una figura
        // ricostruita a partire dalla correlazione.
        std::vector<float> scopePoints;
    };

    // Quanti punti tenere: abbastanza per leggere la figura, pochi
    // abbastanza da non appesantire il WebSocket a ogni invio.
    static constexpr int scopePointCount = 256;

    Analyzer();
    ~Analyzer();

    void prepare(double sampleRate, int blockSize);
    void process(const juce::AudioBuffer<float>& buffer);
    const FFTData& getData() const { return currentData; }
    bool hasNewData() const { return newData; }
    void clearNewData() { newData = false; }

private:
    // FFT
    juce::dsp::FFT fft;
    std::vector<float> fftWindow;
    std::vector<float> fifo[2];
    std::vector<float> fftBuffer[2];
    int fifoIndex = 0;
    int fftSize = 0;
    int hopSize = 0;
    int overlapIndex = 0;
    bool newData = false;
    FFTData currentData;

    // Spectral smoothing (EMA)
    std::vector<float> smoothedMag;
    float smoothCoeff = 0.3f;

    // LUFS
    double sampleRate = 48000.0;

    struct EburDeleter { void operator() (ebur128_state* st) const { if (st != nullptr) ebur128_destroy (&st); } };
    using EburState = std::unique_ptr<ebur128_state, EburDeleter>;
    EburState ebur;
    int loudnessReadCounter = 0;
    std::vector<float> ebur128Interleaved;

    // True peak
    float lastTruePeak = 0.0f;
    float sampleHold = 0.0f;
    int clipHoldFrames = 0;

    // Clipping
    std::atomic<int> clipCount{0};
    float clipThresholdDb = -0.5f;

    // Vectorscope: raccolta a rotazione dei campioni stereo decimati.
    std::vector<float> scopeBuffer;   // interleaved x,y
    int scopeWritePos = 0;
    int scopeDecimation = 8;          // 1 campione ogni N, ricalcolato in prepare()
    int scopeDecimationCounter = 0;

    void applyWindow(float* data, int size);
    void updateLoudness(const juce::AudioBuffer<float>& buffer);
    void detectTruePeak(const juce::AudioBuffer<float>& buffer);
    void collectScopePoints(const juce::AudioBuffer<float>& buffer);

    /** Centroide, rolloff ed energia per banda, dallo spettro appena
        calcolato. Sono misure di forma del suono, non di livello. */
    void computeSpectralShape();

    /** Loudness range: la distanza fra i passaggi piani e quelli forti.
        Si calcola sulla distribuzione dei valori short-term, come da
        EBU R128: decimo percentile contro novantacinquesimo. */
    void updateLoudnessRange();

    // Storico dei valori short-term per il calcolo dell'LRA. Uno ogni
    // secondo circa, tenuti per gli ultimi venti minuti.
    std::deque<float> shortTermHistory;
    int shortTermHistoryCounter = 0;

    // ── Tempo e tonalita' ──────────────────────────────────────────
    //
    //  Entrambi si ricavano dallo spettro che gia' calcoliamo, senza
    //  librerie esterne. Costano piu' delle altre misure, quindi non
    //  girano a ogni frame: vedi i contatori qui sotto.

    /** Accumula il flusso spettrale, cioe' quanto lo spettro cresce da un
        frame al successivo: e' il segnale su cui si leggono gli attacchi. */
    void updateOnsetEnvelope();

    /** Cerca il periodo che si ripete nell'inviluppo degli attacchi e lo
        traduce in battiti al minuto. */
    void estimateTempo();

    /** Somma l'energia per classe di altezza e confronta il risultato con
        i profili di tonalita' maggiore e minore. */
    void estimateKey();

    std::vector<float> previousMagnitudes;   // per il flusso spettrale
    std::deque<float> onsetEnvelope;         // un valore per hop
    int tempoUpdateCounter = 0;
    int keyUpdateCounter = 0;
    std::array<double, 12> chromaAccumulator {};
    double chromaFrames = 0.0;
};
