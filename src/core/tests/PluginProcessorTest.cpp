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
            expect (! processor.hasEditor() == false, "Processor should have an editor");
            expect (processor.getName() == "WhyCremisi VST Plugin", "Processor name should be correct");
        }
        
        beginTest ("Gain parameters");
        {
            WhyCremisiProcessor processor;
            
            // Check that we have the gain parameters
            expect (processor.getParameters().getParameter ("gain1") != nullptr, "gain1 parameter should exist");
            expect (processor.getParameters().getParameter ("gain2") != nullptr, "gain2 parameter should exist");
            
            // Set gain1 to +6dB (which is a gain of 2.0)
            processor.getParameters().getParameterAsValue ("gain1").setValue (6.0);
            // Set gain2 to -6dB (which is a gain of 0.5)
            processor.getParameters().getParameterAsValue ("gain2").setValue (-6.0);
            
            // Create a simple buffer
            juce::AudioBuffer<float> buffer (2, 1024); // 2 channels, 1024 samples
            buffer.clear();
            
            // Add a test signal (e.g., 0.5)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    buffer.setSample (channel, sample, 0.5f);
                }
            }
            
            // Process the buffer
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            
            // Check that the gain has been applied
            // Expected: 0.5 * gain1 * gain2
            // gain1 = 6dB -> factor 2.0
            // gain2 = -6dB -> factor 0.5
            // So total factor = 2.0 * 0.5 = 1.0
            // Therefore, the output should be 0.5
            
            const float tolerance = 0.001f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    float sampleValue = buffer.getSample (channel, sample);
                    expect (std::abs (sampleValue - 0.5f) < tolerance, 
                            "Sample should be 0.5 after applying +6dB and -6dB gains");
                }
            }
        }
        
        beginTest ("Gain zero");
        {
            WhyCremisiProcessor processor;
            
            // Set both gains to 0dB (factor 1.0)
            processor.getParameters().getParameterAsValue ("gain1").setValue (0.0);
            processor.getParameters().getParameterAsValue ("gain2").setValue (0.0);
            
            juce::AudioBuffer<float> buffer (2, 1024);
            buffer.setSample (0, 0, 0.5f); // Just check one sample
            
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            
            expect (std::abs (buffer.getSample (0, 0) - 0.5f) < 0.001f, 
                    "With 0dB gains, signal should be unchanged");
        }
        
        beginTest ("Gain silence");
        {
            WhyCremisiProcessor processor;
            
            // Set gains to -inf (but we can't set -inf, so use a very low value)
            processor.getParameters().getParameterAsValue ("gain1").setValue (-60.0); // approx 0.001
            processor.getParameters().getParameterAsValue ("gain2").setValue (-60.0); // approx 0.001
            
            juce::AudioBuffer<float> buffer (2, 1024);
            buffer.setSample (0, 0, 1.0f);
            
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            
            // With -60dB twice, gain is about 0.001 * 0.001 = 1e-6
            // So 1.0 * 1e-6 = 0.000001
            expect (std::abs (buffer.getSample (0, 0)) < 0.0001f, 
                    "With -60dB gains, signal should be very close to zero");
        }
    }
};

static PluginProcessorTest pluginProcessorTest;