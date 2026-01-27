//==============================================================================
// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

static inline float safeAbs (float x) noexcept { return std::abs (x); }
static inline float clampf (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }

KcompressorAudioProcessor::KcompressorAudioProcessor()
: AudioProcessor (BusesProperties()
                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout KcompressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterFloat>(
        paramDetectorDriveId,
        "Detector Drive (dB)",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f),
        -12.0f));
    return { p.begin(), p.end() };
}

bool KcompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == out) && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void KcompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate);

    // 4x oversampling (2 stages). Use IIR half-band (low CPU, good enough).
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        getTotalNumInputChannels(),
        2, // numStages => 4x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);

    oversampling->reset();
    oversampling->initProcessing ((size_t) samplesPerBlock);

    // reset governor memory
    a1 = b1 = 1.0f;
    a2 = b2 = 1.0f;
    a3 = b3 = 1.0f;
}

void KcompressorAudioProcessor::releaseResources()
{
    oversampling.reset();
}

void KcompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    // Read parameter once per block
    const float detDb  = apvts.getRawParameterValue (paramDetectorDriveId)->load();
    const float detMul = std::pow (10.0f, detDb / 20.0f);

    // Oversample up
    auto block = juce::dsp::AudioBlock<float> (buffer);
    auto osBlock = oversampling->processSamplesUp (block);

    const int osNumSamples = (int) osBlock.getNumSamples();
    const int osNumCh = (int) osBlock.getNumChannels();

    // channel pointers to oversampled data
    auto* ch0 = osBlock.getChannelPointer (0);
    float* ch1 = (osNumCh > 1) ? osBlock.getChannelPointer (1) : nullptr;

    for (int i = 0; i < osNumSamples; ++i)
    {
        const float inL = ch0[i];
        const float inR = (ch1 != nullptr) ? ch1[i] : inL;

        // ---------------- Stage 1 ----------------
        const float s1 = safeAbs (inL + inR) * detMul;
        float inj1 = s1 * s1;
        inj1 = inj1 / (1.0f + injK * inj1);

        const float a1safe = std::max (safeAbs (a1), eps);
        a1 = (1.0f - c1) * (a1 + safeAbs (b1 - a1)) + c1 * inj1 / a1safe;

        const float b1safe = std::max (safeAbs (b1), eps);
        const float e1 = clampf (safeAbs (a1 / b1safe), 0.0f, expoClamp);
        b1 = (1.0f - c1) * (a1 + safeAbs (b1 - a1)) + c1 * safeAbs (std::pow (b1safe, e1));

        const float g1 = 1.0f / std::max (safeAbs (b1), eps);

        float y1L = inL * g1;
        float y1R = inR * g1;

        // ---------------- Stage 2 ----------------
        const float s2 = safeAbs (y1L + y1R) * detMul;
        float inj2 = s2 * s2;
        inj2 = inj2 / (1.0f + injK * inj2);

        const float a2safe = std::max (safeAbs (a2), eps);
        a2 = (1.0f - c2) * (a2 + safeAbs (b2 - a2)) + c2 * inj2 / a2safe;

        const float b2safe = std::max (safeAbs (b2), eps);
        const float e2 = clampf (safeAbs (a2 / b2safe), 0.0f, expoClamp);
        b2 = (1.0f - c2) * (a2 + safeAbs (b2 - a2)) + c2 * safeAbs (std::pow (b2safe, e2));

        const float g2 = 1.0f / std::max (safeAbs (b2), eps);

        float y2L = y1L * g2;
        float y2R = y1R * g2;

        // ---------------- Stage 3 ----------------
        const float s3 = safeAbs (y2L + y2R) * detMul;
        float inj3 = s3 * s3;
        inj3 = inj3 / (1.0f + injK * inj3);

        const float a3safe = std::max (safeAbs (a3), eps);
        a3 = (1.0f - c3) * (a3 + safeAbs (b3 - a3)) + c3 * inj3 / a3safe;

        const float b3safe = std::max (safeAbs (b3), eps);
        const float e3 = clampf (safeAbs (a3 / b3safe), 0.0f, expoClamp);
        b3 = (1.0f - c3) * (a3 + safeAbs (b3 - a3)) + c3 * safeAbs (std::pow (b3safe, e3));

        const float g3 = 1.0f / std::max (safeAbs (b3), eps);

        const float outL = y2L * g3;
        const float outR = y2R * g3;

        ch0[i] = outL;
        if (ch1 != nullptr)
            ch1[i] = outR;
    }

    // downsample back into 'buffer'
    oversampling->processSamplesDown (block);

    // clear extra output channels if any
    for (int ch = 2; ch < numCh; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* KcompressorAudioProcessor::createEditor()
{
    return new KcompressorAudioProcessorEditor (*this);
}

void KcompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void KcompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KcompressorAudioProcessor();
}
