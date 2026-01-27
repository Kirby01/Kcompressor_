//==============================================================================
// PluginEditor.h
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class KcompressorAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit KcompressorAudioProcessorEditor (KcompressorAudioProcessor&);
    ~KcompressorAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    KcompressorAudioProcessor& proc;

    juce::Slider detDrive;
    juce::Label  detLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> detAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KcompressorAudioProcessorEditor)
};
