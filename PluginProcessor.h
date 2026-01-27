//==============================================================================
// PluginProcessor.h
#pragma once
#include <JuceHeader.h>

class KcompressorAudioProcessor final : public juce::AudioProcessor
{
public:
    KcompressorAudioProcessor();
    ~KcompressorAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Kcompressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* paramDetectorDriveId = "detDriveDb";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 4x oversampling (2 stages = 2^2)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Governor state (persist across blocks)
    float a1 = 1.0f, b1 = 1.0f;
    float a2 = 1.0f, b2 = 1.0f;
    float a3 = 1.0f, b3 = 1.0f;

    // constants (match your JSFX)
    static constexpr float eps       = 1.0e-12f;
    static constexpr float expoClamp = 10.0f;
    static constexpr float injK      = 0.02f;
    static constexpr float c1        = 0.0100f;
    static constexpr float c2        = 0.0090f;
    static constexpr float c3        = 0.0085f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KcompressorAudioProcessor)
};
