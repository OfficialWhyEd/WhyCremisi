#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/*
    Stadio stereo: somma in mono, inversione di fase, ampiezza del campo.

    Sono i tre controlli che stanno sotto il vectorscope. Prima esistevano
    solo come pulsanti: premerli mandava un messaggio OSC a
    /whycremisi/mono, /whycremisi/midside e /whycremisi/phaseinvert —
    indirizzi di un namespace inventato che nessuna DAW ascolta. Il pulsante
    si accendeva, il pacchetto partiva verso il nulla e l'audio non cambiava
    di un capello. Un comando che finge di funzionare e' un difetto come un
    numero inventato: fa credere di aver verificato qualcosa.

    WhyCremisi sta sulla traccia e l'audio ce l'ha in mano: queste tre cose
    puo' benissimo farle da se'. Lo stadio sta in fondo alla catena, prima
    dell'analizzatore, cosi' il vectorscope mostra il risultato — si preme
    Mono e la figura collassa sulla verticale, che e' esattamente il
    riscontro che si cerca facendo quella prova.

    I parametri sono atomici: li scrive il thread dei messaggi, li legge
    quello audio.
*/
class StereoStage
{
public:
    // Somma i due canali: serve a sentire come regge il mix in mono.
    void setMonoSum (bool attivo)      { monoSum.store (attivo); }

    // Inverte la fase del canale destro. Se sommando in mono il suono
    // diventa piu' pieno invece che piu' magro, c'era un problema di fase.
    void setPhaseInvert (bool attivo)  { phaseInvert.store (attivo); }

    // Solo il segnale laterale: quello che sta fuori dal centro.
    void setSideOnly (bool attivo)     { sideOnly.store (attivo); }

    // Ampiezza del campo: 0 = mono, 1 = invariato, 2 = doppio.
    void setWidth (float w)            { width.store (juce::jlimit (0.0f, 2.0f, w)); }

    bool  isMonoSum() const     { return monoSum.load(); }
    bool  isPhaseInvert() const { return phaseInvert.load(); }
    bool  isSideOnly() const    { return sideOnly.load(); }
    float getWidth() const      { return width.load(); }

    // Vero quando lo stadio non sta toccando niente: permette di saltarlo
    // del tutto e di lasciare il segnale bit per bit com'era.
    bool isNeutral() const
    {
        return ! monoSum.load() && ! phaseInvert.load() && ! sideOnly.load()
            && std::abs (width.load() - 1.0f) < 1.0e-6f;
    }

    void reset() { monoSum = false; phaseInvert = false; sideOnly = false; width = 1.0f; }

    void process (juce::AudioBuffer<float>& buffer);

private:
    std::atomic<bool>  monoSum { false };
    std::atomic<bool>  phaseInvert { false };
    std::atomic<bool>  sideOnly { false };
    std::atomic<float> width { 1.0f };
};
