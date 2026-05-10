#include "Limiter.h"

void Limiter::prepare(double sr, int blockSize)
{
    sampleRate = sr;
    lookaheadSamples = static_cast<int>(lookaheadMs * sr / 1000.0f);
    if (lookaheadSamples < 1) lookaheadSamples = 1;

    int delaySize = lookaheadSamples + blockSize;
    delayBuffer.clear();
    delayBuffer.resize(2, std::vector<float>(delaySize, 0.0f));
    writePos = 0;
    reset();
}

void Limiter::reset()
{
    envelope = 0.0f;
    currentGainReduction = 0.0f;
    for (auto& ch : delayBuffer)
        std::fill(ch.begin(), ch.end(), 0.0f);
    writePos = 0;
}

void Limiter::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled) return;

    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    float releaseCoeff = std::exp(-1000.0f / (releaseMs * static_cast<float>(sampleRate)));
    float thresholdLinear = juce::Decibels::decibelsToGain(thresholdDb);

    for (int s = 0; s < numSamples; ++s)
    {
        // Lookahead: write current sample to delay buffer
        for (int c = 0; c < numChannels; ++c)
            delayBuffer[c][writePos] = buffer.getSample(c, s);

        // Find peak in lookahead window
        float peak = 0.0f;
        for (int c = 0; c < numChannels; ++c)
        {
            for (int d = 0; d < lookaheadSamples; ++d)
            {
                int idx = (writePos - lookaheadSamples + 1 + d) % (lookaheadSamples + numSamples);
                if (idx < 0) idx += (lookaheadSamples + numSamples);
                peak = juce::jmax(peak, std::abs(delayBuffer[c][idx]));
            }
        }

        // Envelope
        if (peak > envelope)
            envelope = peak;
        else
            envelope = releaseCoeff * (envelope - peak) + peak;

        // Calculate gain reduction
        float gainReduction = 1.0f;
        if (envelope > thresholdLinear)
            gainReduction = thresholdLinear / envelope;

        currentGainReduction = juce::Decibels::gainToDecibels(gainReduction);

        // Read delayed sample and apply gain
        int readPos = (writePos - lookaheadSamples + 1) % (lookaheadSamples + numSamples);
        if (readPos < 0) readPos += (lookaheadSamples + numSamples);

        for (int c = 0; c < numChannels; ++c)
            buffer.setSample(c, s, delayBuffer[c][readPos] * gainReduction);

        writePos = (writePos + 1) % (lookaheadSamples + numSamples);
    }
}
