#include "StereoStage.h"

void StereoStage::process (juce::AudioBuffer<float>& buffer)
{
    // Su materiale mono non c'e' campo stereo da toccare, e l'inversione
    // di fase su un canale solo cambierebbe il segno di tutto senza dire
    // niente a nessuno.
    if (buffer.getNumChannels() < 2 || isNeutral())
        return;

    const bool  mono   = monoSum.load();
    const bool  invert = phaseInvert.load();
    const bool  side   = sideOnly.load();
    const float w      = width.load();

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        float l = L[i];
        float r = R[i];

        // L'inversione va per prima: e' una prova sul segnale in ingresso,
        // e applicarla dopo la somma non mostrerebbe piu' niente.
        if (invert) r = -r;

        // Scomposizione mid/side. Il fattore 0,5 sul mid e la
        // ricomposizione senza fattore tengono il guadagno invariato
        // quando width vale 1: la prova non deve cambiare il volume,
        // altrimenti si giudica il livello invece della fase.
        float m = 0.5f * (l + r);
        float s = 0.5f * (l - r);

        if (side)
        {
            // Solo i lati, riportati su entrambe le uscite per poterli
            // ascoltare centrati.
            L[i] = R[i] = s * 2.0f;
            continue;
        }

        if (mono)
        {
            L[i] = R[i] = m;
            continue;
        }

        s *= w;
        L[i] = m + s;
        R[i] = m - s;
    }
}
