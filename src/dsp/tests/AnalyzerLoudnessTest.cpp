#include "../Analyzer.h"
#include <juce_core/juce_core.h>
#include <cmath>

// Riferimento EBU R128 (EBU Tech 3341): un seno a 1 kHz uguale su due canali a -20 dBFS
// misura -20 LUFS. Un solo canale a pari livello misura 3 dB in meno.
class AnalyzerLoudnessTest : public juce::UnitTest
{
public:
    AnalyzerLoudnessTest() : juce::UnitTest ("AnalyzerLoudnessTest", "whycremisi") {}

    // Fa passare secondi di seno (o silenzio se amp = 0) dividendo in buffer da blockSize.
    static void feed (Analyzer& a, double sr, double seconds, float amp, int blockSize, double& phase)
    {
        juce::AudioBuffer<float> b (2, blockSize);
        const double inc = 2.0 * juce::MathConstants<double>::pi * 1000.0 / sr;
        const int total = static_cast<int> (seconds * sr);
        for (int done = 0; done < total; done += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = amp * static_cast<float> (std::sin (phase));
                phase += inc;
                b.setSample (0, i, v);
                b.setSample (1, i, v);
            }
            a.process (b);
        }
    }

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("Seno a -20 dBFS su due canali: -20 LUFS, qualunque sia la dimensione del buffer");
        for (int blockSize : { 64, 480, 512, 1024 })
        {
            Analyzer a;
            a.prepare (sr, blockSize);
            double ph = 0.0;
            feed (a, sr, 10.0, 0.1f, blockSize, ph);
            const auto& d = a.getData();
            expect (std::abs (d.momentaryLoudness - -20.0f) < 0.2f,
                    "momentary " + juce::String (d.momentaryLoudness) + " con buffer " + juce::String (blockSize));
            expect (std::abs (d.shortTermLoudness - -20.0f) < 0.2f,
                    "short-term " + juce::String (d.shortTermLoudness) + " con buffer " + juce::String (blockSize));
            expect (std::abs (d.integratedLoudness - -20.0f) < 0.2f,
                    "integrato " + juce::String (d.integratedLoudness) + " con buffer " + juce::String (blockSize));
        }

        beginTest ("Il silenzio non abbassa l'integrato (gate assoluto e relativo)");
        {
            Analyzer a;
            a.prepare (sr, 512);
            double ph = 0.0;
            feed (a, sr, 5.0, 0.0f, 512, ph);
            feed (a, sr, 5.0, 0.1f, 512, ph);
            expect (std::abs (a.getData().integratedLoudness - -20.0f) < 0.3f,
                    "integrato " + juce::String (a.getData().integratedLoudness));
        }

        beginTest ("Una parte molto piu' bassa viene esclusa dal gate relativo");
        {
            Analyzer a;
            a.prepare (sr, 512);
            double ph = 0.0;
            feed (a, sr, 6.0, 0.1f, 512, ph);       // -20 LUFS
            feed (a, sr, 6.0, 0.005f, 512, ph);     // 26 dB sotto: fuori dal gate
            expect (std::abs (a.getData().integratedLoudness - -20.0f) < 0.3f,
                    "integrato " + juce::String (a.getData().integratedLoudness));
        }
    }
};

static AnalyzerLoudnessTest analyzerLoudnessTest;
