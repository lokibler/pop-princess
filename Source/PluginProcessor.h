/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/

class CompressorPieceAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    CompressorPieceAudioProcessor();
    ~CompressorPieceAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
  
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout();
    
    APVTS apvts {*this, nullptr, "Parameters", createParameterLayout()};
    
    void updateCompressor ();
    void updateWaveshaper ();
    void updateEQ ();
    
    juce::AudioBuffer<float> inputSig;
    juce::AudioSourceChannelInfo inBuff;
    juce::AudioSourceChannelInfo outBuff;
    
    juce::AudioParameterFloat* amount { nullptr };
    juce::AudioParameterFloat* threshold { nullptr };
    juce::AudioParameterFloat* makeupGain { nullptr };
    
private:
    //==============================================================================
    
    juce::dsp::ProcessSpec spec;
    
    enum
    {
        driveGainIndex,
        waveShaperIndex,
        outGainIndex,
        compressorIndex,
        compGainIndex,
    };
    
    enum
    {
        eqIndex
    };
    
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;
    
    using MBFilter = juce::dsp::LinkwitzRileyFilter<float>;
    MBFilter LP1, AP2, HP1, LP2, HP2;
    std::array<juce::AudioBuffer<float>, 3> MBFilterBuffers;
    
    juce::dsp::ProcessorChain<juce::dsp::Gain<float>, //begin saturator
                              juce::dsp::WaveShaper<float>,
                              juce::dsp::Gain<float>, //end of saturator
                              juce::dsp::Compressor<float>, //begin compressor
                              juce::dsp::Gain<float> //end of compressor
    > processorChain1;
    
    std::array<juce::dsp::Compressor<float>, 3> compressors;
    juce::dsp::Compressor<float>& LowBandComp = compressors[0];
    juce::dsp::Compressor<float>& MidBandComp = compressors[1];
    juce::dsp::Compressor<float>& HighBandComp = compressors[2];
    
    std::array<juce::dsp::Gain<float>, 3> mbCompInGains;
    std::array<juce::dsp::Gain<float>, 3> mbCompOutGains;
    
    juce::dsp::ProcessorChain<
                              juce::dsp::ProcessorDuplicator<Filter, FilterCoefs> //high shelf eq
    > processorChain2;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorPieceAudioProcessor)
};
