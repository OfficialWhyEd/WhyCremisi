#include "Analyzer.h"
#include <cmath>

Analyzer::Analyzer() : fft(11)  // 2048-point FFT
{
    fftSize = 2048;
    hopSize = fftSize / 2;  // 50% overlap
}

Analyzer::~Analyzer() = default;

void Analyzer::initKWeightingFilters()
{
    // K-weighting per EBU R128 / ITU-R BS.1770
    // Pre-filter: 2nd-order high-pass at ~38Hz (Butterworth)
    // Shelving filter: 2nd-order shelving with +4dB at ~1.5kHz

    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };

    preFilter.prepare(spec);
    *preFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.0f);

    shelveFilter.prepare(spec);
    *shelveFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1500.0, 0.5, 4.0f);
}

void Analyzer::prepare(double sr, int)
{
    sampleRate = sr;
    initKWeightingFilters();

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

    // LUFS window: 400ms momentary
    momentaryWindow = static_cast<int>(sampleRate * 0.4);
    momentarySumSq = 0.0;
    momentaryCount = 0;

    // Short-term LUFS: 3s blocks (100ms blocks, 75% overlap)
    shortTermBlockSize = static_cast<int>(sampleRate * 0.1);  // 100ms
    int numBlocks = static_cast<int>(sampleRate * 3.0 / shortTermBlockSize);  // ~30 blocks for 3s
    shortTermBlocks.assign(numBlocks, 0.0);
    shortTermBlockCount = 0;
    shortTermSumSq = 0.0;
    shortTermPos = 0;

    // Integrated LUFS
    integratedSumSq = 0.0;
    integratedWeightedSumSq = 0.0;
    integratedBlockCount = 0.0;
    integratedWeightedBlockCount = 0.0;

    lastTruePeak = -96.0f;
    clipCount.store(0);

    newData = false;
}

void Analyzer::process(const juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    // Update loudness and true peak for this buffer
    updateLoudness(buffer);
    detectTruePeak(buffer);

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

    // Pre-filter each channel through K-weighting
    juce::AudioBuffer<float> weightedBuffer(2, numSamples);
    for (int c = 0; c < juce::jmin(numChannels, 2); ++c)
    {
        auto* src = buffer.getReadPointer(c);
        auto* dst = weightedBuffer.getWritePointer(c);
        for (int s = 0; s < numSamples; ++s)
        {
            float x = src[s];
            x = preFilter.processSample(x);
            x = shelveFilter.processSample(x);
            dst[s] = x;
        }
    }

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

    // LUFS: compute sum of squares of weighted signal
    double blockSumSq = 0.0;
    for (int c = 0; c < juce::jmin(numChannels, 2); ++c)
    {
        auto* src = weightedBuffer.getReadPointer(c);
        for (int s = 0; s < numSamples; ++s)
            blockSumSq += static_cast<double>(src[s]) * src[s];
    }

    // Accumulate for momentary (400ms)
    momentarySumSq += blockSumSq;
    momentaryCount += numSamples;

    if (momentaryCount >= momentaryWindow)
    {
        int used = juce::jmin(momentaryCount, momentaryWindow);
        double meanSq = momentarySumSq / (used * 2);  // 2 channels
        currentData.momentaryLoudness = (meanSq > 1e-12)
            ? static_cast<float>(-0.691 + 10.0 * std::log10(meanSq))
            : -96.0f;
        momentarySumSq = 0.0;
        momentaryCount = 0;
    }

    // Short-term: accumulate 100ms blocks in a 3s sliding window
    shortTermSumSq += blockSumSq;
    shortTermBlockCount += numSamples;

    if (shortTermBlockCount >= shortTermBlockSize)
    {
        int sz = shortTermBlocks.size();
        double oldVal = shortTermBlocks[shortTermPos];
        shortTermBlocks[shortTermPos] = shortTermSumSq;
        shortTermPos = (shortTermPos + 1) % sz;

        double total = 0.0;
        for (auto& v : shortTermBlocks) total += v;

        double meanSq = total / (sz * shortTermBlockSize * 2);  // 2 channels
        currentData.shortTermLoudness = (meanSq > 1e-12)
            ? static_cast<float>(-0.691 + 10.0 * std::log10(meanSq))
            : -96.0f;

        shortTermSumSq = 0.0;
        shortTermBlockCount = 0;
    }

    // Integrated: accumulate with gating
    integratedSumSq += blockSumSq;
    integratedBlockCount += numSamples;

    double totalSamples = integratedBlockCount * 2;  // 2 channels
    double meanSq = integratedSumSq / totalSamples;

    if (meanSq > 1e-12)
    {
        // Gating: only count blocks > -70 LUFS relative to total
        double relThreshold = std::pow(10.0, (-70.0 - 0.691) / 10.0);  // -70 LUFS absolute
        double blockThreshold = relThreshold * totalSamples;

        if (blockSumSq > blockThreshold)
        {
            integratedWeightedSumSq += blockSumSq;
            integratedWeightedBlockCount += numSamples;
        }

        double weightedMeanSq = integratedWeightedSumSq / (integratedWeightedBlockCount * 2.0 + 1e-12);
        currentData.integratedLoudness = (weightedMeanSq > 1e-12)
            ? static_cast<float>(-0.691 + 10.0 * std::log10(weightedMeanSq))
            : -96.0f;
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
