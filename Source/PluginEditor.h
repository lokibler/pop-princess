/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct Colors
{
    juce::Colour mylightPink = juce::Colour(225,201,205);
    juce::Colour mymedPink = juce::Colour(210,155,168);
    juce::Colour mypink = juce::Colour(209,132,150);
    juce::Colour mydarkPink = juce::Colour(183,107,126);
    juce::Colour mybrown = juce::Colour(129,78,67);
};

enum
{
    fftOrder = 11,
    fftSize = 1 << fftOrder,
    scopeSize = 512
};

class myAnalyzer   : public juce::AudioAppComponent,
                            private juce::Timer
{
public:
    myAnalyzer(CompressorPieceAudioProcessor&);

    ~myAnalyzer() override
    {
        shutdownAudio();
    }

    //==============================================================================
    void prepareToPlay (int, double) override {}
    void releaseResources() override          {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        drawFrame (g);
    }

    void timerCallback() override;

    void pushNextSampleIntoFifo (float, int) noexcept;

    void drawNextFrameOfSpectrum();
    void drawFrame (juce::Graphics &g);

private:
    CompressorPieceAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (myAnalyzer)

    juce::dsp::FFT fftOut;
    juce::dsp::WindowingFunction<float> windowOut;
    float fifoOut[fftSize];
    float fftDataOut[2*fftSize];
    int fifoOutIndex = 0;
    bool nextReadyOut = false;
    float scopeDataOut[scopeSize];

    juce::dsp::FFT fftIn;
    juce::dsp::WindowingFunction<float> windowIn;
    float fifoIn[fftSize];
    float fftDataIn[2*fftSize];
    int fifoInIndex = 0;
    bool nextReadyIn = false;
    float scopeDataIn[scopeSize];

    Colors mycolors;
};



//==============================================================================
/**
*/
class CompressorPieceAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    CompressorPieceAudioProcessorEditor (CompressorPieceAudioProcessor&);
    ~CompressorPieceAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    CompressorPieceAudioProcessor& audioProcessor;
    
    juce::Slider threshDial, makeupDial, masterDial;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> threshAttach, makeupAttach, masterAttach;

    myAnalyzer analyzer { audioProcessor };

    Colors mycolors;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorPieceAudioProcessorEditor)
};
