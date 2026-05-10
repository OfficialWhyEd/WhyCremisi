#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class Limiter
{
public:
    void setThreshold(float db) { thresholdDb = db; }
    void setRelease(float ms) { releaseMs = ms; }
    void setLookahead(float ms) { lookaheadMs = ms; }
    void setEnabled(bool e) { enabled = e; }

    float getThreshold() const { return thresholdDb; }
    float getRelease() const { return releaseMs; }
    float getLookahead() const { return lookaheadMs; }
    bool isEnabled() const { return enabled; }
    float getCurrentGainReduction() const { return currentGainReduction; }

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

private:
    float thresholdDb = -6.0f;
    float releaseMs = 50.0f;
    float lookaheadMs = 2.0f;
    bool enabled = true;

    double sampleRate = 44100.0;
    float envelope = 0.0f;
    float currentGainReduction = 0.0f;
    int lookaheadSamples = 0;
    std::vector<std::vector<float>> delayBuffer;
    int writePos = 0;
};
