#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class Compressor
{
public:
    void setThreshold(float db) { thresholdDb = db; }
    void setRatio(float r) { ratio = r; }
    void setAttack(float ms) { attackMs = ms; }
    void setRelease(float ms) { releaseMs = ms; }
    void setKnee(float db) { kneeDb = db; }
    void setMakeup(float db) { makeupDb = db; }
    void setEnabled(bool e) { enabled = e; }

    float getThreshold() const { return thresholdDb; }
    float getRatio() const { return ratio; }
    float getAttack() const { return attackMs; }
    float getRelease() const { return releaseMs; }
    float getKnee() const { return kneeDb; }
    float getMakeup() const { return makeupDb; }
    bool isEnabled() const { return enabled; }
    float getCurrentReduction() const { return currentReduction; }

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

private:
    float thresholdDb = -24.0f;
    float ratio = 4.0f;
    float attackMs = 5.0f;
    float releaseMs = 100.0f;
    float kneeDb = 6.0f;
    float makeupDb = 0.0f;
    bool enabled = true;

    double sampleRate = 44100.0;
    float envelope = 0.0f;
    float currentReduction = 0.0f;
};
