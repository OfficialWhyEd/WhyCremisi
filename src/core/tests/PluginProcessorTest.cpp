/*
  ==============================================================================
    PluginProcessorTest.cpp
    Unit tests for WhyCremisiPluginProcessor
  ==============================================================================
*/

#include "../../PluginProcessor.h"
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessorTest : public juce::UnitTest
{
public:
    PluginProcessorTest() : juce::UnitTest ("PluginProcessorTest", juce::UnitTestCategory::audio) {}
    
    void runTest() override
    {
        beginTest ("Initial state");
        {
            WhyCremisiProcessor processor;
            
            // Check that the processor is created successfully
            expect (processor.hasEditor(), "Processor should have an editor");
            expect (processor.getName() == "WhyCremisi VST Plugin", "Processor name should be correct");
        }
        
        beginTest ("Gain parameters");
        {
            WhyCremisiProcessor processor;
            
            // Check that we have the gain parameters
            expect (processor.getParameters().getParameter ("gain1") != nullptr, "gain1 parameter should exist");
            expect (processor.getParameters().getParameter ("gain2") != nullptr, "gain2 parameter should exist");
            
            // Set gain1 to +6dB using normalized value: (6 - (-60)) / 72 = 0.917
            auto* p1 = processor.getParameters().getParameter ("gain1");
            p1->setValueNotifyingHost (p1->convertTo0to1 (6.0f));
            
            // Create a simple buffer
            juce::AudioBuffer<float> buffer (2, 1024);
            buffer.clear();
            
            // Add a test signal (0.5)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    buffer.setSample (channel, sample, 0.5f);
            
            // Process the buffer
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            
            // gainParam1 (+6dB = factor ~2.0) is applied via smoothedGain
            // Expected: 0.5 * ~2.0 ≈ 1.0
            const float tolerance = 0.01f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    float sampleValue = buffer.getSample (channel, sample);
                    expect (std::abs (sampleValue - 1.0f) < tolerance, 
                            "Sample should be ~1.0 after applying +6dB gain");
                }
            }
        }
        
        beginTest ("Gain zero");
        {
            WhyCremisiProcessor processor;
            
            // Set gain1 to 0dB (factor 1.0) using normalized value: (0 - (-60)) / 72 = 0.833
            auto* p1 = processor.getParameters().getParameter ("gain1");
            p1->setValueNotifyingHost (p1->convertTo0to1 (0.0f));
            
            juce::AudioBuffer<float> buffer (2, 1024);
            buffer.setSample (0, 0, 0.5f);
            
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            
            expect (std::abs (buffer.getSample (0, 0) - 0.5f) < 0.01f, 
                    "With 0dB gain, signal should be unchanged");
        }
        
        beginTest ("Gain silence");
        {
            WhyCremisiProcessor processor;
            
            // Set gain1 to -60dB (factor ~0.001) using normalized value: (-60 - (-60)) / 72 = 0.0
            auto* p1 = processor.getParameters().getParameter ("gain1");
            p1->setValueNotifyingHost (p1->convertTo0to1 (-60.0f));
            
            juce::AudioBuffer<float> buffer (2, 1024);
            buffer.setSample (0, 0, 1.0f);
            
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            
            // With -60dB, gain factor is ~0.001
            // So 1.0 * 0.001 = 0.001
            expect (std::abs (buffer.getSample (0, 0)) < 0.01f, 
                    "With -60dB gain, signal should be near zero");
        }
    }
};

static PluginProcessorTest pluginProcessorTest;