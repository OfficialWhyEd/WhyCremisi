#include "EQBand.h"

void EQBand::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = 2;
    filter.prepare(spec);
    updateCoefficients(sampleRate);
    needsUpdate = false;
}

void EQBand::reset() { filter.reset(); }

void EQBand::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled) return;
    if (needsUpdate) { updateCoefficients(currentSampleRate); needsUpdate = false; }

    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto context = juce::dsp::ProcessContextReplacing<float>(block);
    filter.process(context);
}

void EQBand::updateCoefficients(double sampleRate)
{
    auto coeff = filter.state;
    switch (type)
    {
        case Type::Lowpass:
            *coeff = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, freq, q);
            break;
        case Type::Highpass:
            *coeff = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freq, q);
            break;
        case Type::Bandpass:
            *coeff = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, freq, q);
            break;
        case Type::Lowshelf:
            *coeff = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
            break;
        case Type::Highshelf:
            *coeff = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
            break;
        case Type::Peak:
            *coeff = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
            break;
        case Type::Notch:
            *coeff = *juce::dsp::IIR::Coefficients<float>::makeNotch(sampleRate, freq, q);
            break;
    }
}

float EQBand::getMagnitudeAt(float, double) const
{
    if (!enabled) return 0.0f;
    if (type == Type::Peak || type == Type::Lowshelf || type == Type::Highshelf)
        return gain;
    return 0.0f;
}
