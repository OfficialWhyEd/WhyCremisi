/*
  ==============================================================================
    OscBridgeTest.cpp
    Unit tests for OscBridge
  ==============================================================================
*/

#include "../OscBridge.h"
#include <juce_core/juce_core.h>

class OscBridgeTest : public juce::UnitTest
{
public:
    OscBridgeTest() : juce::UnitTest ("OscBridgeTest", juce::UnitTestCategory::audio) {}
    
    void runTest() override
    {
        beginTest ("Construction and destruction");
        {
            // Test that we can create and destroy an OscBridge
            OscBridge bridge (9000, 8080);
            // If we get here without throwing, the test passes
        }
        
        beginTest ("Port getters");
        {
            OscBridge bridge (9001, 8081);
            expect (bridge.getOscPort() == 9001, "OSC port should be 9001");
            expect (bridge.getWebSocketPort() == 8081, "WebSocket port should be 8081");
        }
        
        beginTest ("Initial state");
        {
            OscBridge bridge (9000, 8080);
            expect (! bridge.isRunning(), "Bridge should not be running initially");
        }
    }
};

static OscBridgeTest oscBridgeTest;