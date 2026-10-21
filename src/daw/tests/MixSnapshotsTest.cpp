#include "../MixSnapshots.h"
#include "../AbletonDawHandler.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>

class MixSnapshotsTest : public juce::UnitTest
{
public:
    MixSnapshotsTest() : juce::UnitTest ("MixSnapshotsTest", "whycremisi") {}

    void runTest() override
    {
        beginTest ("Il volume di Live torna in dB, inverso di quello che scriviamo");
        {
            expect (std::abs (MixSnapshots::dbFromLiveVolume (0.85)) < 0.001);
            expect (std::abs (MixSnapshots::dbFromLiveVolume (1.0) - 6.0) < 0.001);
            expect (MixSnapshots::dbFromLiveVolume (0.0) <= -100.0);
            for (float db : { -30.0f, -12.0f, -6.0f, -1.0f, 0.0f, 3.0f, 6.0f })
            {
                const float v = AbletonDawHandler::liveVolumeFromGain (juce::Decibels::decibelsToGain (db));
                expect (std::abs (MixSnapshots::dbFromLiveVolume (v) - db) < 0.05,
                        "andata e ritorno a " + juce::String (db) + " dB");
            }
        }

        beginTest ("Le istantanee sopravvivono al disco e si tengono separate per nome");
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("whycremisi-mix-test.json");
            f.deleteFile();
            MixSnapshots s;
            s.setFile (f);
            expect (! s.save ("", { { 0, 0.0, 0.0, false } }), "nome vuoto rifiutato");
            expect (! s.save ("vuoto", {}), "nessuna traccia rifiutata");
            expect (s.save ("prima", { { 0, -6.5, -0.25, false }, { 1, -12.0, 0.5, true } }));
            expect (s.save ("seconda", { { 0, 0.0, 0.0, false } }));

            MixSnapshots altro;
            altro.setFile (f);
            std::vector<MixTrackState> t;
            expect (altro.recall ("prima", t) && t.size() == 2);
            if (t.size() == 2)
            {
                expect (t[0].volumeDb == -6.5 && t[0].pan == -0.25 && ! t[0].mute);
                expect (t[1].index == 1 && t[1].mute && t[1].pan == 0.5);
            }
            expect (altro.names().size() == 2);
            expect (! altro.recall ("non-esiste", t));
            f.deleteFile();
        }
    }
};

static MixSnapshotsTest mixSnapshotsTest;
