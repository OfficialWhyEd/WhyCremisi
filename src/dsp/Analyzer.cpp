#include "Analyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

Analyzer::Analyzer() : fft(11)  // 2048-point FFT
{
    fftSize = 2048;
    hopSize = fftSize / 2;  // 50% overlap
}

Analyzer::~Analyzer() = default;

void Analyzer::prepare(double sr, int)
{
    sampleRate = sr;

    // Hann window
    fftWindow.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
        fftWindow[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    // Apply correction factor for 50% overlap: sum of hann^2 weights
    float overlapCorrection = 0.0f;
    for (int i = 0; i < fftSize; ++i)
        overlapCorrection += fftWindow[i] * fftWindow[i];
    // Divide by hop size for overlap-add normalization
    float normFactor = static_cast<float>(hopSize) / overlapCorrection;

    fifo[0].resize(fftSize, 0.0f);
    fifo[1].resize(fftSize, 0.0f);
    fftBuffer[0].resize(fftSize * 2, 0.0f);
    fftBuffer[1].resize(fftSize * 2, 0.0f);
    fifoIndex = 0;

    currentData.magnitudes.resize(fftSize / 2, 0.0f);
    currentData.rawMagnitudes.resize(fftSize / 2, 0.0f);
    currentData.phases.resize(fftSize / 2, 0.0f);
    smoothedMag.resize(fftSize / 2, 0.0f);

    // Vectorscope: la decimazione e' scelta perche' i punti raccolti
    // coprano all'incirca 50 ms di audio, che e' una finestra abbastanza
    // corta da seguire il movimento e abbastanza lunga da disegnare una
    // figura leggibile.
    scopeBuffer.assign (scopePointCount * 2, 0.0f);
    scopeWritePos = 0;
    scopeDecimationCounter = 0;
    scopeDecimation = juce::jmax (1, static_cast<int> ((sampleRate * 0.05) / scopePointCount));
    currentData.scopePoints.assign (scopePointCount * 2, 0.0f);

    // LUFS secondo EBU R128 / ITU-R BS.1770-4: libebur128 (MIT) in modo
    // HISTOGRAM, cosi' non alloca nulla per blocco sul thread audio.
    ebur = EburState (ebur128_init (2, static_cast<unsigned long> (sampleRate),
                                    EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_HISTOGRAM));
    loudnessReadCounter = 0;
    ebur128Interleaved.assign (16384, 0.0f);

    lastTruePeak = -96.0f;
    clipCount.store(0);

    newData = false;
}

void Analyzer::computeSpectralShape()
{
    const int bins = fftSize / 2;
    if (bins <= 0 || (int) currentData.magnitudes.size() < bins) return;

    const float binHz = (float) (sampleRate / fftSize);

    // Centroide: media delle frequenze pesata sull'energia. Un valore alto
    // dice suono brillante, uno basso suono scuro. E' la misura piu'
    // compatta per descrivere il timbro con un numero solo.
    double energiaTotale = 0.0, sommaPesata = 0.0;
    for (int i = 1; i < bins; ++i)
    {
        const double e = (double) currentData.magnitudes[i] * currentData.magnitudes[i];
        energiaTotale += e;
        sommaPesata   += e * (i * binHz);
    }

    if (energiaTotale < 1e-12)
    {
        currentData.spectralCentroid = 0.0f;
        currentData.spectralRolloff = 0.0f;
        currentData.bandEnergy.fill (0.0f);
        return;
    }

    currentData.spectralCentroid = (float) (sommaPesata / energiaTotale);

    // Rolloff: la frequenza sotto cui sta l'85% dell'energia. Dice fin dove
    // arriva davvero il contenuto, al netto della coda in alto.
    const double soglia = energiaTotale * 0.85;
    double accumulata = 0.0;
    currentData.spectralRolloff = (float) (bins * binHz);
    for (int i = 1; i < bins; ++i)
    {
        accumulata += (double) currentData.magnitudes[i] * currentData.magnitudes[i];
        if (accumulata >= soglia)
        {
            currentData.spectralRolloff = i * binHz;
            break;
        }
    }

    // Dieci bande a spaziatura logaritmica, da 20 Hz a 20 kHz: e' come
    // l'orecchio divide lo spettro, mentre bande a spaziatura lineare
    // metterebbero nove decimi dei numeri sopra i 2 kHz.
    static constexpr float bandaMin = 20.0f, bandaMax = 20000.0f;
    const float passo = std::log10 (bandaMax / bandaMin) / FFTData::numBands;

    std::array<double, FFTData::numBands> perBanda {};
    for (int i = 1; i < bins; ++i)
    {
        const float f = i * binHz;
        if (f < bandaMin || f > bandaMax) continue;
        int b = (int) (std::log10 (f / bandaMin) / passo);
        b = juce::jlimit (0, FFTData::numBands - 1, b);
        perBanda[(size_t) b] += (double) currentData.magnitudes[i] * currentData.magnitudes[i];
    }

    for (int b = 0; b < FFTData::numBands; ++b)
        currentData.bandEnergy[(size_t) b] = (float) (perBanda[(size_t) b] / energiaTotale * 100.0);
}

void Analyzer::updateLoudnessRange()
{
    // Un campione al secondo circa: l'LRA descrive l'andamento del brano,
    // non le variazioni istantanee, e tenerne di piu' non cambia il numero.
    if (++shortTermHistoryCounter < 20) return;
    shortTermHistoryCounter = 0;

    const float st = currentData.shortTermLoudness;
    // Sotto questa soglia c'e' silenzio o quasi: includerlo gonfierebbe
    // l'LRA facendo sembrare dinamico un pezzo che semplicemente ha delle
    // pause.
    if (st < -70.0f) return;

    shortTermHistory.push_back (st);
    if (shortTermHistory.size() > 1200)   // circa venti minuti
        shortTermHistory.pop_front();

    if (shortTermHistory.size() < 10) return;

    std::vector<float> ordinati (shortTermHistory.begin(), shortTermHistory.end());
    std::sort (ordinati.begin(), ordinati.end());

    const auto percentile = [&ordinati](double p) {
        const size_t idx = (size_t) juce::jlimit<double> (0.0, ordinati.size() - 1.0,
                                                          p * (ordinati.size() - 1));
        return ordinati[idx];
    };

    // EBU R128: distanza fra decimo e novantacinquesimo percentile.
    currentData.loudnessRange = percentile (0.95) - percentile (0.10);
}

// ═══════════════════════════════════════════════════════════════════
//  Tempo e tonalita'
// ═══════════════════════════════════════════════════════════════════

void Analyzer::updateOnsetEnvelope()
{
    const int bins = fftSize / 2;
    if ((int) currentData.rawMagnitudes.size() < bins) return;

    if ((int) previousMagnitudes.size() != bins)
    {
        previousMagnitudes.assign ((size_t) bins, 0.0f);
        return;   // il primo frame non ha un precedente con cui confrontarsi
    }

    // Flusso spettrale a mezza onda: si sommano solo gli aumenti di
    // energia. Le diminuzioni si scartano perche' un suono che si spegne
    // non e' un attacco, e contarle riempirebbe l'inviluppo di rumore.
    double flusso = 0.0;
    for (int i = 1; i < bins; ++i)
    {
        const float delta = currentData.rawMagnitudes[(size_t) i] - previousMagnitudes[(size_t) i];
        if (delta > 0.0f) flusso += delta;
        previousMagnitudes[(size_t) i] = currentData.rawMagnitudes[(size_t) i];
    }

    onsetEnvelope.push_back ((float) flusso);

    // Dieci secondi di storia: abbastanza per riconoscere un tempo lento
    // senza tenere in memoria l'intero brano.
    const double framesAlSecondo = sampleRate / (double) hopSize;
    const size_t massimo = (size_t) (framesAlSecondo * 10.0);
    while (onsetEnvelope.size() > massimo)
        onsetEnvelope.pop_front();
}

void Analyzer::estimateTempo()
{
    // L'autocorrelazione costa: la si fa ogni due secondi circa, non a
    // ogni frame. Il tempo di un brano non cambia cosi' in fretta.
    const double framesAlSecondo = sampleRate / (double) hopSize;
    if (++tempoUpdateCounter < (int) (framesAlSecondo * 2.0)) return;
    tempoUpdateCounter = 0;

    const size_t n = onsetEnvelope.size();
    if (n < (size_t) (framesAlSecondo * 4.0)) return;   // meno di 4 s: troppo poco

    std::vector<float> env (onsetEnvelope.begin(), onsetEnvelope.end());

    // Si toglie la media: senza, l'autocorrelazione e' dominata dal livello
    // assoluto e il picco cade sempre a ritardo zero.
    const double media = std::accumulate (env.begin(), env.end(), 0.0) / (double) n;
    for (auto& v : env) v -= (float) media;

    // Si cercano solo i periodi che corrispondono a tempi musicali
    // plausibili, fra 60 e 200 BPM.
    const int ritardoMin = (int) (framesAlSecondo * 60.0 / 200.0);
    const int ritardoMax = (int) (framesAlSecondo * 60.0 / 60.0);
    if (ritardoMax >= (int) n || ritardoMin < 1) return;

    double energia = 0.0;
    for (float v : env) energia += (double) v * v;
    if (energia < 1e-9) return;

    std::vector<double> correlazioni ((size_t) (ritardoMax - ritardoMin + 1), 0.0);
    double miglioreValore = 0.0;
    int miglioreRitardo = 0;
    for (int lag = ritardoMin; lag <= ritardoMax; ++lag)
    {
        double somma = 0.0;
        for (size_t i = 0; i + (size_t) lag < n; ++i)
            somma += (double) env[i] * env[i + (size_t) lag];

        correlazioni[(size_t) (lag - ritardoMin)] = somma;
        if (somma > miglioreValore)
        {
            miglioreValore = somma;
            miglioreRitardo = lag;
        }
    }

    if (miglioreRitardo <= 0) return;

    // Il ritardo e' un numero intero di frame, e questo limita molto la
    // precisione: a 170 BPM un frame in piu' o in meno sposta la stima di
    // una decina di battiti. Interpolando una parabola sui tre punti
    // attorno al massimo si recupera la posizione frazionaria del picco.
    double ritardoFine = (double) miglioreRitardo;
    const int idx = miglioreRitardo - ritardoMin;
    if (idx > 0 && idx + 1 < (int) correlazioni.size())
    {
        const double y0 = correlazioni[(size_t) (idx - 1)];
        const double y1 = correlazioni[(size_t) idx];
        const double y2 = correlazioni[(size_t) (idx + 1)];
        const double denom = y0 - 2.0 * y1 + y2;
        if (std::abs (denom) > 1e-12)
        {
            const double offset = 0.5 * (y0 - y2) / denom;
            if (std::abs (offset) <= 1.0)
                ritardoFine += offset;
        }
    }

    currentData.bpm = (float) (60.0 * framesAlSecondo / ritardoFine);
    // Quanto il picco spicca rispetto all'energia totale: se il segnale non
    // ha una pulsazione chiara il valore resta basso e chi legge lo sa.
    currentData.bpmConfidence = (float) juce::jlimit (0.0, 1.0, miglioreValore / energia);
}

void Analyzer::estimateKey()
{
    const int bins = fftSize / 2;
    if ((int) currentData.magnitudes.size() < bins) return;

    const double binHz = sampleRate / fftSize;

    // Ogni bin finisce nella sua classe di altezza. Si parte da 55 Hz
    // (La1) e si sale: sotto, la risoluzione della FFT non basta a
    // distinguere semitoni vicini.
    for (int i = 1; i < bins; ++i)
    {
        const double f = i * binHz;
        if (f < 55.0 || f > 5000.0) continue;

        // 69 e' il numero MIDI del La440; il modulo 12 da' la classe.
        const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
        int classe = ((int) std::lround (midi)) % 12;
        if (classe < 0) classe += 12;

        const double e = (double) currentData.magnitudes[(size_t) i] * currentData.magnitudes[(size_t) i];
        chromaAccumulator[(size_t) classe] += e;
    }
    chromaFrames += 1.0;

    const double framesAlSecondo = sampleRate / (double) hopSize;
    if (++keyUpdateCounter < (int) (framesAlSecondo * 2.0)) return;
    keyUpdateCounter = 0;
    if (chromaFrames < framesAlSecondo) return;   // meno di un secondo: presto

    double totale = 0.0;
    for (double v : chromaAccumulator) totale += v;
    if (totale < 1e-12) return;

    std::array<double, 12> profilo {};
    for (size_t i = 0; i < 12; ++i)
    {
        profilo[i] = chromaAccumulator[i] / totale;
        currentData.chroma[i] = (float) profilo[i];
    }

    // Profili di Krumhansl-Kessler: quanto pesa ogni grado della scala in
    // musica tonale, ricavati da esperimenti di ascolto. Confrontando il
    // chroma con questi, ruotati sulle dodici tonalita', si trova quella
    // che somiglia di piu'.
    static constexpr double maggiore[12] =
        { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static constexpr double minore[12] =
        { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    static const char* nomi[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const auto correlazione = [&profilo](const double* rif, int rotazione)
    {
        double mediaA = 0.0, mediaB = 0.0;
        for (int i = 0; i < 12; ++i) { mediaA += profilo[(size_t) i]; mediaB += rif[i]; }
        mediaA /= 12.0; mediaB /= 12.0;

        double num = 0.0, denA = 0.0, denB = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const double a = profilo[(size_t) ((i + rotazione) % 12)] - mediaA;
            const double b = rif[i] - mediaB;
            num += a * b; denA += a * a; denB += b * b;
        }
        return (denA > 0.0 && denB > 0.0) ? num / std::sqrt (denA * denB) : 0.0;
    };

    double migliore = -2.0;
    int miglioreTonica = 0;
    bool miglioreMinore = false;

    for (int t = 0; t < 12; ++t)
    {
        const double cMag = correlazione (maggiore, t);
        if (cMag > migliore) { migliore = cMag; miglioreTonica = t; miglioreMinore = false; }

        const double cMin = correlazione (minore, t);
        if (cMin > migliore) { migliore = cMin; miglioreTonica = t; miglioreMinore = true; }
    }

    currentData.key = juce::String (nomi[miglioreTonica]) + (miglioreMinore ? " min" : " maj");
    currentData.keyConfidence = (float) juce::jlimit (0.0, 1.0, migliore);
}

void Analyzer::collectScopePoints (const juce::AudioBuffer<float>& buffer)
{
    if (scopeBuffer.empty()) return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0) return;

    const float* left  = buffer.getReadPointer (0);
    const float* right = (numChannels > 1) ? buffer.getReadPointer (1) : left;

    for (int s = 0; s < numSamples; ++s)
    {
        if (++scopeDecimationCounter < scopeDecimation)
            continue;
        scopeDecimationCounter = 0;

        // Rotazione di 45 gradi: e' quello che distingue un vectorscope da
        // un semplice grafico L contro R. Il fattore 0.7071 (1/sqrt2) tiene
        // i valori dentro -1..1 invece di farli arrivare a +-2.
        const float l = left[s];
        const float r = right[s];
        const float side = (l - r) * 0.70710678f;
        const float mid  = (l + r) * 0.70710678f;

        scopeBuffer[scopeWritePos * 2]     = juce::jlimit (-1.0f, 1.0f, side);
        scopeBuffer[scopeWritePos * 2 + 1] = juce::jlimit (-1.0f, 1.0f, mid);
        scopeWritePos = (scopeWritePos + 1) % scopePointCount;
    }
}

void Analyzer::process(const juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    // Senza prepare() i buffer interni hanno dimensione zero e ogni accesso
    // finisce fuori dai limiti. Un host che rispetta le regole chiama sempre
    // prepareToPlay prima di processBlock, ma non tutti lo fanno in ogni
    // situazione — e un plugin non deve andare in crash per questo.
    if (fftSize <= 0 || (int) fifo[0].size() < fftSize)
        return;

    // Update loudness and true peak for this buffer
    updateLoudness(buffer);
    detectTruePeak(buffer);
    collectScopePoints(buffer);

    for (int s = 0; s < numSamples; ++s)
    {
        for (int c = 0; c < numChannels; ++c)
            fifo[c][fifoIndex] = buffer.getSample(c, s) * fftWindow[fifoIndex];

        fifoIndex++;

        if (fifoIndex >= hopSize)
        {
            // Shift FIFO: keep remaining samples for overlap
            for (int c = 0; c < numChannels; ++c)
            {
                std::copy(fifo[c].begin() + hopSize, fifo[c].end(), fifo[c].begin());
                std::fill(fifo[c].begin() + (fftSize - hopSize), fifo[c].end(), 0.0f);
            }

            // Copy windowed FIFO to FFT buffer
            for (int c = 0; c < numChannels; ++c)
            {
                for (int i = 0; i < fftSize; ++i)
                    fftBuffer[c][i] = fifo[c][i];

                std::fill(fftBuffer[c].begin() + fftSize, fftBuffer[c].end(), 0.0f);
                fft.performRealOnlyForwardTransform(fftBuffer[c].data(), false);
            }

            // Extract magnitudes with inter-channel averaging
            for (int i = 0; i < fftSize / 2; ++i)
            {
                float mag = 0.0f;
                for (int c = 0; c < numChannels; ++c)
                {
                    float re = fftBuffer[c][i * 2];
                    float im = fftBuffer[c][i * 2 + 1];
                    mag += std::sqrt(re * re + im * im);
                }
                mag /= numChannels;

                currentData.rawMagnitudes[i] = mag;

                // EMA smoothing
                smoothedMag[i] = smoothCoeff * mag + (1.0f - smoothCoeff) * smoothedMag[i];
                currentData.magnitudes[i] = smoothedMag[i];

                currentData.phases[i] = (numChannels > 1)
                    ? std::atan2(fftBuffer[1][i * 2 + 1], fftBuffer[1][i * 2])
                    : std::atan2(fftBuffer[0][i * 2 + 1], fftBuffer[0][i * 2]);
            }

            // Punti del vectorscope: si copiano riordinandoli dal piu'
            // vecchio al piu' recente, altrimenti la figura risulterebbe
            // spezzata nel punto in cui il buffer circolare si richiude.
            if (! scopeBuffer.empty())
            {
                for (int i = 0; i < scopePointCount; ++i)
                {
                    const int src = (scopeWritePos + i) % scopePointCount;
                    currentData.scopePoints[i * 2]     = scopeBuffer[src * 2];
                    currentData.scopePoints[i * 2 + 1] = scopeBuffer[src * 2 + 1];
                }
            }

            computeSpectralShape();
            updateLoudnessRange();
            updateOnsetEnvelope();
            estimateTempo();
            estimateKey();

            fifoIndex = 0;
            newData = true;
        }
    }
}

void Analyzer::updateLoudness(const juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (numSamples == 0) return;

    // Raw RMS
    float rmsSumSq = 0.0f;
    for (int c = 0; c < juce::jmin(numChannels, 2); ++c)
    {
        auto* src = buffer.getReadPointer(c);
        for (int s = 0; s < numSamples; ++s)
            rmsSumSq += src[s] * src[s];
    }
    currentData.rms = (rmsSumSq > 1e-10f)
        ? juce::Decibels::gainToDecibels(std::sqrt(rmsSumSq / (numSamples * numChannels)))
        : -96.0f;

    // LUFS: libebur128 vede ogni campione, il risultato non dipende dalla
    // dimensione del buffer. Un canale solo viene duplicato (mono su 2 canali).
    if (ebur != nullptr)
    {
        // libebur128 vuole i campioni intercalati; il vettore e' riservato in prepare().
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (numChannels > 1 ? 1 : 0);
        if (static_cast<int> (ebur128Interleaved.size()) < numSamples * 2)
            ebur128Interleaved.resize (static_cast<size_t> (numSamples) * 2);
        for (int i = 0; i < numSamples; ++i)
        {
            ebur128Interleaved[static_cast<size_t> (i) * 2]     = l[i];
            ebur128Interleaved[static_cast<size_t> (i) * 2 + 1] = r[i];
        }
        ebur128_add_frames_float (ebur.get(), ebur128Interleaved.data(), static_cast<size_t> (numSamples));

        loudnessReadCounter += numSamples;
        if (loudnessReadCounter >= static_cast<int> (sampleRate * 0.1))   // ogni 100 ms
        {
            loudnessReadCounter = 0;
            auto leggi = [this] (int (*f) (ebur128_state*, double*), float& dove)
            {
                double v = 0.0;
                if (f (ebur.get(), &v) == EBUR128_SUCCESS && std::isfinite (v))
                    dove = static_cast<float> (v);
                else
                    dove = -96.0f;
            };
            leggi (ebur128_loudness_momentary, currentData.momentaryLoudness);
            leggi (ebur128_loudness_shortterm, currentData.shortTermLoudness);
            leggi (ebur128_loudness_global,    currentData.integratedLoudness);
        }
    }

    // Stereo correlation (Pearson)
    if (numChannels >= 2)
    {
        const auto* l = buffer.getReadPointer(0);
        const auto* r = buffer.getReadPointer(1);
        float sumL = 0.0f, sumR = 0.0f, sumLR = 0.0f, sumL2 = 0.0f, sumR2 = 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            float lv = l[s], rv = r[s];
            sumL += lv; sumR += rv;
            sumLR += lv * rv;
            sumL2 += lv * lv;
            sumR2 += rv * rv;
        }
        float n = static_cast<float>(numSamples);
        float denom = std::sqrt((sumL2 - sumL * sumL / n) * (sumR2 - sumR * sumR / n));
        currentData.corrMono = (denom > 1e-10f) ? (sumLR - sumL * sumR / n) / denom : 0.0f;
    }
}

void Analyzer::detectTruePeak(const juce::AudioBuffer<float>& buffer)
{
    // 2x oversampling via linear interpolation for true peak estimation
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    float maxSample = 0.0f;

    for (int c = 0; c < numChannels; ++c)
    {
        const auto* src = buffer.getReadPointer(c);
        for (int s = 0; s < numSamples - 1; ++s)
        {
            // Check original sample
            float a = std::abs(src[s]);
            if (a > maxSample) maxSample = a;

            // Interpolated midpoint (simple linear)
            float b = std::abs((src[s] + src[s + 1]) * 0.5f);
            if (b > maxSample) maxSample = b;
        }
        // Check last sample
        float last = std::abs(src[numSamples - 1]);
        if (last > maxSample) maxSample = last;
    }

    // Decay envelope (hold for 100ms, then slowly decay)
    const int holdSamples = static_cast<int>(sampleRate * 0.1);
    if (maxSample >= lastTruePeak)
    {
        lastTruePeak = maxSample;
        clipHoldFrames = holdSamples;
    }
    else if (clipHoldFrames > 0)
    {
        clipHoldFrames--;
    }
    else
    {
        // Decay at ~10dB/sec
        lastTruePeak *= 1.0f - (1.0f / static_cast<float>(sampleRate)) * 5.0f;
    }

    currentData.truePeak = juce::Decibels::gainToDecibels(lastTruePeak);

    // Clipping count
    if (maxSample > juce::Decibels::decibelsToGain(clipThresholdDb))
        clipCount.fetch_add(1);

    currentData.clippingCount = clipCount.load();
}

void Analyzer::applyWindow(float* data, int size)
{
    for (int i = 0; i < size; ++i)
        data[i] *= fftWindow[i];
}
