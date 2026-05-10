#include "Analyzer.h"
#include <cmath>

Analyzer::Analyzer() : fft(11)  // 2048-point FFT
{
    fftSize = 2048;
}

Analyzer::~Analyzer() = default;

void Analyzer::prepare(double, int)
{
    // Hann window
    fftWindow.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
        fftWindow[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    fifo[0].resize(fftSize, 0.0f);
    fifo[1].resize(fftSize, 0.0f);
    fftBuffer[0].resize(fftSize * 2, 0.0f);
    fftBuffer[1].resize(fftSize * 2, 0.0f);
    fifoIndex = 0;
    currentData.magnitudes.resize(fftSize / 2, 0.0f);
    currentData.phases.resize(fftSize / 2, 0.0f);
}

void Analyzer::process(const juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    for (int s = 0; s < numSamples; ++s)
    {
        for (int c = 0; c < numChannels; ++c)
            fifo[c][fifoIndex] = buffer.getSample(c, s);

        fifoIndex++;

        if (fifoIndex >= fftSize)
        {
            fifoIndex = 0;

            // Perform FFT on each channel
            for (int c = 0; c < numChannels; ++c)
            {
                for (int i = 0; i < fftSize; ++i)
                    fftBuffer[c][i] = fifo[c][i] * fftWindow[i];

                std::fill(fftBuffer[c].begin() + fftSize, fftBuffer[c].end(), 0.0f);
                fft.performRealOnlyForwardTransform(fftBuffer[c].data(), false);
            }

            // Extract magnitudes (average both channels)
            for (int i = 0; i < fftSize / 2; ++i)
            {
                float mag = 0.0f;
                for (int c = 0; c < numChannels; ++c)
                {
                    float re = fftBuffer[c][i * 2];
                    float im = fftBuffer[c][i * 2 + 1];
                    mag += std::sqrt(re * re + im * im);
                }
                currentData.magnitudes[i] = mag / numChannels;
                currentData.phases[i] = std::atan2(
                    fftBuffer[1][i * 2 + 1], fftBuffer[1][i * 2]);
            }

            newData = true;
        }
    }
}

float Analyzer::calculateCorrelation(const juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    if (numSamples < 1 || buffer.getNumChannels() < 2) return 0.0f;

    float sumL = 0.0f, sumR = 0.0f, sumLR = 0.0f, sumL2 = 0.0f, sumR2 = 0.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        float l = buffer.getSample(0, s);
        float r = buffer.getSample(1, s);
        sumL += l; sumR += r;
        sumLR += l * r;
        sumL2 += l * l;
        sumR2 += r * r;
    }

    float denom = std::sqrt((sumL2 - sumL * sumL / numSamples) * (sumR2 - sumR * sumR / numSamples));
    if (denom < 1e-10f) return 0.0f;
    return (sumLR - sumL * sumR / numSamples) / denom;
}

float Analyzer::calculateLoudness(const std::vector<float>& channel)
{
    float sumSq = 0.0f;
    for (auto s : channel) sumSq += s * s;
    if (sumSq < 1e-10f) return -96.0f;
    return juce::Decibels::gainToDecibels(std::sqrt(sumSq / channel.size()));
}
