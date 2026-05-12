#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>

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
    };

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
    double momentarySumSq = 0.0;
    int momentaryCount = 0;
    int momentaryWindow = 0;  // samples for 400ms

    juce::dsp::IIR::Filter<float> preFilter;     // K-weighting pre-filter
    juce::dsp::IIR::Filter<float> shelveFilter;  // K-weighting shelving

    // Short-term LUFS (3s sliding window)
    std::vector<double> shortTermBlocks;
    int shortTermBlockSize = 0;
    int shortTermBlockCount = 0;
    double shortTermSumSq = 0.0;
    int shortTermPos = 0;

    // Integrated LUFS
    double integratedSumSq = 0.0;
    double integratedWeightedSumSq = 0.0;
    double integratedBlockCount = 0.0;
    double integratedWeightedBlockCount = 0.0;

    // True peak
    float lastTruePeak = 0.0f;
    float sampleHold = 0.0f;
    int clipHoldFrames = 0;

    // Clipping
    std::atomic<int> clipCount{0};
    float clipThresholdDb = -0.5f;

    void applyWindow(float* data, int size);
    void updateLoudness(const juce::AudioBuffer<float>& buffer);
    void detectTruePeak(const juce::AudioBuffer<float>& buffer);
    void initKWeightingFilters();
};
