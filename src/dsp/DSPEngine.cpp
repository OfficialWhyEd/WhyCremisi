#include "DSPEngine.h"

DSPEngine::DSPEngine()
{
    // Create 4 default EQ bands
    for (int i = 0; i < 4; ++i)
        eqBands.push_back(std::make_unique<EQBand>());

    compressor = std::make_unique<Compressor>();
    limiter = std::make_unique<Limiter>();
    analyzer = std::make_unique<Analyzer>();
}

DSPEngine::~DSPEngine() = default;

void DSPEngine::prepare(double sampleRate, int blockSize)
{
    for (auto& band : eqBands)
        band->prepare(sampleRate, blockSize);
    compressor->prepare(sampleRate, blockSize);
    limiter->prepare(sampleRate, blockSize);
    analyzer->prepare(sampleRate, blockSize);
}

void DSPEngine::reset()
{
    for (auto& band : eqBands)
        band->reset();
    compressor->reset();
    limiter->reset();
}

void DSPEngine::process(juce::AudioBuffer<float>& buffer)
{
    // EQ
    if (!bypass[EqModule])
    {
        for (auto& band : eqBands)
            band->process(buffer);
    }

    // Compressor
    if (!bypass[CompModule])
        compressor->process(buffer);

    // Limiter
    if (!bypass[LimitModule])
        limiter->process(buffer);

    // Analyzer (always active for metering)
    analyzer->process(buffer);
}

void DSPEngine::setBypass(int module, bool bypassed)
{
    if (module >= 0 && module < 3)
        bypass[module] = bypassed;
}
