#include "Compressor.h"

void Compressor::prepare(double sr, int) { sampleRate = sr; reset(); }
void Compressor::reset() { envelope = 0.0f; currentReduction = 0.0f; }

void Compressor::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled) return;

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    float attackCoeff = std::exp(-1000.0f / (attackMs * static_cast<float>(sampleRate)));
    float releaseCoeff = std::exp(-1000.0f / (releaseMs * static_cast<float>(sampleRate)));

    for (int s = 0; s < numSamples; ++s)
    {
        float sum = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            sum += std::abs(buffer.getSample(c, s));
        float input = sum / static_cast<float>(numChannels);

        // Envelope follower
        if (input > envelope)
            envelope = attackCoeff * (envelope - input) + input;
        else
            envelope = releaseCoeff * (envelope - input) + input;

        // Convert to dB
        float envDb = juce::Decibels::gainToDecibels(envelope + 1e-6f);

        // Soft knee compression
        float kneeHalf = kneeDb * 0.5f;
        float reduction = 0.0f;
        float overshoot = envDb - thresholdDb;

        if (overshoot <= -kneeHalf)
            reduction = 0.0f;
        else if (overshoot >= kneeHalf)
            reduction = overshoot * (1.0f - 1.0f / ratio);
        else
        {
            // In knee zone
            float x = (overshoot + kneeHalf) / kneeDb;
            reduction = x * x * kneeHalf * (1.0f - 1.0f / ratio) / 2.0f;
        }

        currentReduction = reduction;
        float gainReduction = juce::Decibels::decibelsToGain(-reduction + makeupDb);

        for (int c = 0; c < numChannels; ++c)
            buffer.setSample(c, s, buffer.getSample(c, s) * gainReduction);
    }
}
