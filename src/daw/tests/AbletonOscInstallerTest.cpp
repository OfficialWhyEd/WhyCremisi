#include "../AbletonOscInstaller.h"
#include <juce_core/juce_core.h>

class AbletonOscInstallerTest : public juce::UnitTest
{
public:
    AbletonOscInstallerTest() : juce::UnitTest ("AbletonOscInstallerTest", "whycremisi") {}

    static juce::MemoryBlock makeZip (const std::vector<std::pair<juce::String, juce::String>>& files)
    {
        juce::ZipFile::Builder b;
        std::vector<std::unique_ptr<juce::TemporaryFile>> tmp;
        for (auto& f : files)
        {
            tmp.push_back (std::make_unique<juce::TemporaryFile>());
            tmp.back()->getFile().replaceWithText (f.second);
            b.addFile (tmp.back()->getFile(), 0, f.first);
        }
        juce::MemoryOutputStream out;
        b.writeToStream (out, nullptr);
        return out.getMemoryBlock();
    }

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("whycremisi-osc-test-" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
        root.createDirectory();

        beginTest ("Uno zip di GitHub si installa in Remote Scripts/AbletonOSC senza test e client");
        {
            auto zip = makeZip ({ { "AbletonOSC-abc/__init__.py", "x" },
                                  { "AbletonOSC-abc/manager.py", "y" },
                                  { "AbletonOSC-abc/abletonosc/song.py", "z" },
                                  { "AbletonOSC-abc/tests/t.py", "t" } });
            juce::MemoryInputStream in (zip, false);
            expect (! AbletonOscInstaller::isInstalled (root));
            auto r = AbletonOscInstaller::installFromZip (in, root);
            expect (r.ok, r.message);
            expect (AbletonOscInstaller::isInstalled (root));
            expect (root.getChildFile ("AbletonOSC/abletonosc/song.py").existsAsFile());
            expect (! root.getChildFile ("AbletonOSC/tests").exists());
            expectEquals (r.filesWritten, 3);
            expect (r.message.contains ("Control Surface"), "Deve dire l'ultimo passo manuale");
        }

        beginTest ("Se AbletonOSC c'e' gia' non viene sovrascritto");
        {
            auto zip = makeZip ({ { "A/__init__.py", "n" }, { "A/manager.py", "n" } });
            juce::MemoryInputStream in (zip, false);
            auto r = AbletonOscInstaller::installFromZip (in, root);
            expect (! r.ok);
            expectEquals (root.getChildFile ("AbletonOSC/__init__.py").loadFileAsString(), juce::String ("x"));
        }

        beginTest ("Uno zip senza i file di AbletonOSC viene rifiutato e non lascia nulla");
        {
            const auto altro = root.getChildFile ("altro");
            auto zip = makeZip ({ { "A/readme.txt", "n" } });
            juce::MemoryInputStream in (zip, false);
            auto r = AbletonOscInstaller::installFromZip (in, altro);
            expect (! r.ok);
            expect (! altro.getChildFile ("AbletonOSC").exists());
        }

        beginTest ("Il commit e' fissato e l'indirizzo punta a quello");
        {
            expectEquals (AbletonOscInstaller::pinnedCommit().length(), 40);
            expect (AbletonOscInstaller::downloadUrl().contains (AbletonOscInstaller::pinnedCommit()));
        }

        beginTest ("La copia inclusa nel plugin si installa senza rete e ascolta solo su localhost");
        {
            const auto dove = root.getChildFile ("inclusa");
            dove.createDirectory();
            auto r = AbletonOscInstaller::installBundled (dove);
            expect (r.ok, r.message);
            expect (AbletonOscInstaller::isInstalled (dove));
            expect (dove.getChildFile ("AbletonOSC/LICENSE.md").existsAsFile());
            expect (! dove.getChildFile ("AbletonOSC/tests").exists());
            const auto server = dove.getChildFile ("AbletonOSC/abletonosc/osc_server.py").loadFileAsString();
            expect (server.contains ("('127.0.0.1', OSC_LISTEN_PORT)"));
            expect (! server.contains ("('0.0.0.0', OSC_LISTEN_PORT)"));
        }

        root.deleteRecursively();
    }
};

static AbletonOscInstallerTest abletonOscInstallerTest;
