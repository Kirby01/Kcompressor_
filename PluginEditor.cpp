//==============================================================================
// PluginEditor.cpp
#include "PluginEditor.h"

static void setupKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
}

KcompressorAudioProcessorEditor::KcompressorAudioProcessorEditor (KcompressorAudioProcessor& p)
: AudioProcessorEditor (&p), proc (p)
{
    setupKnob (detDrive);
    detDrive.setRange (-20.0, 20.0, 0.1);
    detDrive.setSkewFactorFromMidPoint (0.0);
    addAndMakeVisible (detDrive);

    detLabel.setText ("Detector Drive (dB)", juce::dontSendNotification);
    detLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (detLabel);

    detAttach = std::make_unique<Attachment> (proc.apvts, KcompressorAudioProcessor::paramDetectorDriveId, detDrive);

    setSize (260, 200);
}

void KcompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawText ("Kcompressor", getLocalBounds().removeFromTop (28), juce::Justification::centred);
}

void KcompressorAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (28);

    detLabel.setBounds (r.removeFromTop (20));
    detDrive.setBounds (r.removeFromTop (150));
}
