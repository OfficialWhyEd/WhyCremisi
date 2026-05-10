#pragma once

#include "EQBand.h"
#include "Compressor.h"
#include "Limiter.h"
#include "Analyzer.h"
#include <memory>
#include <vector>

class DSPEngine
{
public:
    DSPEngine();
    ~DSPEngine();

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    void setBypass(int module, bool bypassed);

    // Modules
    std::vector<std::unique_ptr<EQBand>> eqBands;
    std::unique_ptr<Compressor> compressor;
    std::unique_ptr<Limiter> limiter;
    std::unique_ptr<Analyzer> analyzer;

    // Module indices for bypass
    enum Module { EqModule = 0, CompModule = 1, LimitModule = 2 };

private:
    bool bypass[3] = { false, false, false };
};
