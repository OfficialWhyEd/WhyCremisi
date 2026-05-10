#pragma once

#include <juce_dsp/juce_dsp.h>

class EQBand
{
public:
    enum class Type { Lowpass, Highpass, Bandpass, Lowshelf, Highshelf, Peak, Notch };

    void setType(Type t) { type = t; needsUpdate = true; }
    void setFrequency(float freqHz) { freq = freqHz; needsUpdate = true; }
    void setQ(float qVal) { q = qVal; needsUpdate = true; }
    void setGain(float gainDb) { gain = gainDb; needsUpdate = true; }
    void setEnabled(bool e) { enabled = e; }

    Type getType() const { return type; }
    float getFrequency() const { return freq; }
    float getQ() const { return q; }
    float getGain() const { return gain; }
    bool isEnabled() const { return enabled; }

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    float getMagnitudeAt(float freqHz, double sampleRate) const;

private:
    void updateCoefficients(double sampleRate);

    Type type = Type::Peak;
    float freq = 1000.0f;
    float q = 0.707f;
    float gain = 0.0f;
    bool enabled = true;
    bool needsUpdate = true;
    double currentSampleRate = 44100.0;

    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;
    Filter filter;
};
