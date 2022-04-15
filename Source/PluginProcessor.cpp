/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CompressorPieceAudioProcessor::CompressorPieceAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    amount = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("Amount"));
    jassert(amount != nullptr);
    
    threshold = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("Threshold"));
    jassert(amount != nullptr);
    
    makeupGain = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("Makeup"));
    jassert(amount != nullptr);
    
    inBuff = juce::AudioSourceChannelInfo();
    outBuff = juce::AudioSourceChannelInfo();
}

CompressorPieceAudioProcessor::~CompressorPieceAudioProcessor()
{
}

//==============================================================================
const juce::String CompressorPieceAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CompressorPieceAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CompressorPieceAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CompressorPieceAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CompressorPieceAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CompressorPieceAudioProcessor::getNumPrograms()
{
    return 1;
}

int CompressorPieceAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CompressorPieceAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String CompressorPieceAudioProcessor::getProgramName (int index)
{
    return {};
}

void CompressorPieceAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void CompressorPieceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    
    //filter
    auto& filter = processorChain2.get<eqIndex>();
    filter.state = FilterCoefs::makeHighShelf(spec.sampleRate, 2500.0f, 0.71f, juce::Decibels::decibelsToGain(0.0f));
    
    processorChain1.prepare(spec);
    processorChain2.prepare(spec);
    
    //saturator begin
    auto& gain1 = processorChain1.get<driveGainIndex>();
    gain1.reset();
    
    auto& waveShaper = processorChain1.get<waveShaperIndex>();
    waveShaper.reset();
    waveShaper.functionToUse = [] (float x)
    {
        /*std::tanh(x);*/ /*juce::jlimit(-1.0f, 1.0f, x);*/
        /*std::signbit(x)*(1.0-0.25/(std::abs(x)+0.25));*/
        float f = 0;
        if(abs(x) > 2/3)
            f = signbit(x);
        else
            f = sin(3*M_PI*x/4);
        return f;
    };
    
    auto& gain2 = processorChain1.get<outGainIndex>();
    gain2.reset();
    //saturator end
    
    //compressor begin
    auto& compressor = processorChain1.get<compressorIndex>();
    compressor.reset();
    compressor.setRatio(1.15f);
    compressor.setAttack(1.0f);
    compressor.setRelease(30.0f);
    compressor.setThreshold(0.0f);
    
    auto& gain3 = processorChain1.get<compGainIndex>();
    gain3.reset();
    //compressor end
    
    //mb compressor begin
    LP1.prepare(spec);
    HP1.prepare(spec);
    AP2.prepare(spec);
    LP2.prepare(spec);
    HP2.prepare(spec);
    LP1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    AP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    LP2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    for(auto& buffer : MBFilterBuffers)
    {
        buffer.setSize(spec.numChannels, samplesPerBlock);
    }
    LP1.setCutoffFrequency(88.3f);
    HP1.setCutoffFrequency(88.3f);
    AP2.setCutoffFrequency(2500.0f);
    LP2.setCutoffFrequency(2500.0f);
    HP2.setCutoffFrequency(2500.0f);
    
    LowBandComp.prepare(spec);
    LowBandComp.reset();
    LowBandComp.setRatio(66.7f);
    LowBandComp.setAttack(47.8f);
    LowBandComp.setRelease(282.0f);
    LowBandComp.setThreshold(-33.8f);
    
    MidBandComp.prepare(spec);
    MidBandComp.reset();
    MidBandComp.setRatio(66.7f);
    MidBandComp.setAttack(22.4f);
    MidBandComp.setRelease(282.0f);
    MidBandComp.setThreshold(-30.2f);
    
    HighBandComp.prepare(spec);
    HighBandComp.reset();
    HighBandComp.setRatio(100.0f);
    HighBandComp.setAttack(13.5f);
    HighBandComp.setRelease(282.0f);
    HighBandComp.setThreshold(-35.5f);
    
    for( size_t i = 0; i < 3; ++i )
    {
        mbCompInGains[i].prepare(spec);
        mbCompInGains[i].reset();
        mbCompInGains[i].setGainDecibels(5.20f);
        mbCompOutGains[i].prepare(spec);
        mbCompOutGains[i].reset();
    }
    mbCompOutGains[0].setGainDecibels(10.3f);
    mbCompOutGains[1].setGainDecibels(5.7f);
    mbCompOutGains[2].setGainDecibels(10.3f);
    //mb compressor end
}

void CompressorPieceAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CompressorPieceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void CompressorPieceAudioProcessor::updateCompressor ()
{
    auto& compressor = processorChain1.get<compressorIndex>();
    compressor.setRatio(amount->get() / 100.0 * (4.0-1.15) + 1.15);
    compressor.setThreshold(threshold->get());
    
    auto& gain = processorChain1.get<compGainIndex>();
    gain.setGainDecibels(makeupGain->get());
}

void CompressorPieceAudioProcessor::updateWaveshaper ()
{
    auto& driveGain = processorChain1.get<driveGainIndex>();
    driveGain.setGainDecibels(amount->get() / 100.0 * 35.0);
    
    auto& outGain = processorChain1.get<outGainIndex>();
    outGain.setGainDecibels(amount->get() / 100.0 * (0-35.0));
}

void CompressorPieceAudioProcessor::updateEQ ()
{
    auto& eq = processorChain2.get<eqIndex>();
    eq.state = FilterCoefs::makeHighShelf(spec.sampleRate, 2500.0f, 0.71f, juce::Decibels::decibelsToGain(amount->get() / 100.0 * (0-0.87)));
}

void CompressorPieceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    inputSig = buffer;
    inBuff = juce::AudioSourceChannelInfo(inputSig);
    
    juce::dsp::AudioBlock<float> block (buffer);
    
    updateWaveshaper();
    updateCompressor();
    updateEQ();
    
    processorChain1.process(juce::dsp::ProcessContextReplacing <float> (block));
    
    // mb comp begin
    for(auto& fb : MBFilterBuffers)
    {
        fb = buffer;
    }
    
    auto fb0Block = juce::dsp::AudioBlock<float>(MBFilterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(MBFilterBuffers[1]);
    auto fb2Block = juce::dsp::AudioBlock<float>(MBFilterBuffers[2]);
    
    auto fb0Ctx = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Ctx = juce::dsp::ProcessContextReplacing<float>(fb1Block);
    auto fb2Ctx = juce::dsp::ProcessContextReplacing<float>(fb2Block);
    
    LP1.process(fb0Ctx);
    AP2.process(fb0Ctx);

    HP1.process(fb1Ctx);
    MBFilterBuffers[2] = MBFilterBuffers[1];
    LP2.process(fb1Ctx);

    HP2.process(fb2Ctx);
    
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();
    
    for( size_t i = 0; i < MBFilterBuffers.size(); ++i )
    {
        auto compblock = juce::dsp::AudioBlock<float>(MBFilterBuffers[i]);
        auto compcontext = juce::dsp::ProcessContextReplacing<float>(compblock);
        mbCompInGains[i].process(compcontext);
        compressors[i].process(compcontext);
        mbCompOutGains[i].process(compcontext);
    }
    
    auto addFilterBand = [nc = numChannels, ns = numSamples](auto& inputBuffer, const auto& source, float &mix)
    {
        for(auto i = 0; i < nc; ++i)
        {
            inputBuffer.addFrom(i, 0, source, i, 0, ns, mix);
        }
    };
    
    float ottMix = amount->get() / 100.0 * 0.75;
    
    addFilterBand(buffer, MBFilterBuffers[0], ottMix);
    addFilterBand(buffer, MBFilterBuffers[1], ottMix);
    addFilterBand(buffer, MBFilterBuffers[2], ottMix);
    //mb comp end
    
    juce::dsp::AudioBlock<float> block2 (buffer);
    
    processorChain2.process(juce::dsp::ProcessContextReplacing <float> (block2));

    outBuff = juce::AudioSourceChannelInfo(buffer);
}

//==============================================================================
bool CompressorPieceAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* CompressorPieceAudioProcessor::createEditor()
{
    return new CompressorPieceAudioProcessorEditor(*this);
    //return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void CompressorPieceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void CompressorPieceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if( tree.isValid() )
    {
        apvts.replaceState(tree);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout CompressorPieceAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    
    using namespace juce;
    
    layout.add(std::make_unique<AudioParameterFloat>("Amount",
                                                     "Amount",
                                                     NormalisableRange<float>(0, 100, 0.1),
                                                     0));
    
    layout.add(std::make_unique<AudioParameterFloat>("Threshold",
                                                     "Threshold",
                                                     NormalisableRange<float>(-70, 6, 0.1),
                                                     0));
    
    layout.add(std::make_unique<AudioParameterFloat>("Makeup",
                                                     "Makeup",
                                                     NormalisableRange<float>(0, 20, 0.01),
                                                     0));
    
    return layout;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressorPieceAudioProcessor();
}
