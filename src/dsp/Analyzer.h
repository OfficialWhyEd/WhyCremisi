#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

class Analyzer
{
public:
    struct FFTData {
        std::vector<float> magnitudes;
        std::vector<float> phases;
        float corrMono = 0.0f;    // phase correlation (-1 to 1)
        float loudness = 0.0f;    // momentary LUFS
    };

    Analyzer();
    ~Analyzer();

    void prepare(double sampleRate, int blockSize);
    void process(const juce::AudioBuffer<float>& buffer);
    const FFTData& getData() const { return currentData; }
    bool hasNewData() const { return newData; }
    void clearNewData() { newData = false; }

private:
    juce::dsp::FFT fft;
    std::vector<float> fftWindow;
    std::vector<float> fifo[2];
    std::vector<float> fftBuffer[2];
    int fifoIndex = 0;
    int fftSize = 0;
    bool newData = false;
    FFTData currentData;

    float calculateCorrelation(const juce::AudioBuffer<float>& buffer);
    float calculateLoudness(const std::vector<float>& channel);
    void applyWindow(float* data, int size);
};
