/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

myAnalyzer::myAnalyzer(CompressorPieceAudioProcessor& p)
                        : audioProcessor (p), fftOut(fftOrder),
                          windowOut(fftSize, juce::dsp::WindowingFunction<float>::hann),
                          fftIn(fftOrder),
                          windowIn(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    setAudioChannels (2, 0);
    startTimerHz (30);
}

void myAnalyzer::pushNextSampleIntoFifo (float sample, int i) noexcept
{
    if(i == 0)
    {
        if (fifoInIndex == fftSize)
        {
            if (!nextReadyIn)
            {
                juce::zeromem (fftDataIn, sizeof (fftDataIn));
                memcpy (fftDataIn, fifoIn, sizeof (fifoIn));
                nextReadyIn = true;
            }

            fifoInIndex = 0;
        }

        fifoIn[fifoInIndex++] = sample;
    }else{
        if (fifoOutIndex == fftSize)
        {
            if (!nextReadyOut)
            {
                juce::zeromem (fftDataOut, sizeof (fftDataOut));
                memcpy (fftDataOut, fifoOut, sizeof (fifoOut));
                nextReadyOut = true;
            }

            fifoOutIndex = 0;
        }

        fifoOut[fifoOutIndex++] = sample;
    }
}

void myAnalyzer::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (audioProcessor.inBuff.buffer->getNumChannels() > 0)
    {
        auto* channelData = audioProcessor.inBuff.buffer->getReadPointer (0, audioProcessor.inBuff.startSample);

        for (auto i = 0; i < audioProcessor.inBuff.numSamples; ++i)
            pushNextSampleIntoFifo (channelData[i], 0);
    }
    if(audioProcessor.outBuff.buffer->getNumChannels() > 0)
    {
        auto* channelData = audioProcessor.outBuff.buffer->getReadPointer (0, audioProcessor.outBuff.startSample);

        for (auto i = 0; i < audioProcessor.outBuff.numSamples; ++i)
            pushNextSampleIntoFifo (channelData[i], 1);
    }
}

void myAnalyzer::drawNextFrameOfSpectrum()
{
    windowIn.multiplyWithWindowingTable (fftDataIn, fftSize);
    windowOut.multiplyWithWindowingTable (fftDataOut, fftSize);

    fftIn.performFrequencyOnlyForwardTransform (fftDataIn);
    fftOut.performFrequencyOnlyForwardTransform (fftDataOut);

    auto mindB = -100.0f;
    auto maxdB =    0.0f;

    for (int i = 0; i < scopeSize; ++i)
    {
        auto skewedProportionX = 1.0f - std::exp (std::log (1.0f - (float) i / (float) scopeSize) * 0.2f);
        auto fftDataIndex = juce::jlimit (0, fftSize / 2, (int) (skewedProportionX * (float) fftSize * 0.5f));
        auto levelIn = juce::jmap (juce::jlimit (mindB, maxdB, juce::Decibels::gainToDecibels (fftDataIn[fftDataIndex])
                                                           - juce::Decibels::gainToDecibels ((float) fftSize)),
                                 mindB, maxdB, 0.0f, 1.0f);
        auto levelOut = juce::jmap (juce::jlimit (mindB, maxdB, juce::Decibels::gainToDecibels (fftDataOut[fftDataIndex])
                                                           - juce::Decibels::gainToDecibels ((float) fftSize)),
                                 mindB, maxdB, 0.0f, 1.0f);
        scopeDataIn[i] = levelIn;
        scopeDataOut[i] = levelOut;
    }
}

void myAnalyzer::timerCallback()
{
    if (nextReadyIn)
    {
        drawNextFrameOfSpectrum();
        nextReadyIn = false;
        repaint();
    }
    if(nextReadyOut)
    {
        drawNextFrameOfSpectrum();
        nextReadyOut = false;
        repaint();
    }
}

void myAnalyzer::drawFrame(juce::Graphics &g)
{
    for (int i = 1; i < scopeSize; ++i)
    {
        auto width  = 380;
        auto height = getLocalBounds().getHeight();

        g.setColour(mycolors.mypink);
        auto startX = (float) juce::jmap (i - 1, 0, scopeSize - 1, 65, width);
        auto startY = juce::jmap (scopeDataIn[i - 1], 0.0f, 1.0f, (float) height, 162.0f);
        auto endX = (float) juce::jmap (i,     0, scopeSize - 1, 65, width);
        auto endY = juce::jmap (scopeDataIn[i],     0.0f, 1.0f, (float) height, 162.0f);
        if(startX <= width && startY <= height && endX <= width && endY <= height
           && startX >= 65.0f && startY >= 162.0f && endX >= 65.0f && endY >= 162.0f)
        {
            g.drawLine ({ startX, startY, endX, endY });
        }
        
        g.setColour(mycolors.mylightPink);
        startX = (float) juce::jmap (i - 1, 0, scopeSize - 1, 65, width);
        startY = juce::jmap (scopeDataOut[i - 1], 0.0f, 1.0f, (float) height, 162.0f);
        endX = (float) juce::jmap (i,     0, scopeSize - 1, 65, width);
        endY = juce::jmap (scopeDataOut[i],     0.0f, 1.0f, (float) height, 162.0f);
        if(startX <= width && startY <= height && endX <= width && endY <= height
           && startX >= 65.0f && startY >= 162.0f && endX >= 65.0f && endY >= 162.0f)
        {
            g.drawLine ({ startX, startY, endX, endY });
        }
        
        g.setColour(mycolors.mybrown);
        float thresh = juce::jmap(pow(10.0f, ((float)audioProcessor.threshold->get())/20.0f), 0.0f, 1.0f, (float)height, 162.0f);
        if(thresh < 162.0f){
            thresh = 162.0f;
        }
        g.drawLine((float)65, thresh, (float)380, thresh);
    }
}

//==============================================================================
CompressorPieceAudioProcessorEditor::CompressorPieceAudioProcessorEditor (CompressorPieceAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (450, 750);

    addAndMakeVisible(analyzer);
    addAndMakeVisible(&masterDial);
    addAndMakeVisible(&threshDial);
    addAndMakeVisible(&makeupDial);
    
    masterAttach = std::make_unique<Attachment>(audioProcessor.apvts,"Amount",masterDial);
    jassert(masterAttach != nullptr);
    threshAttach = std::make_unique<Attachment>(audioProcessor.apvts,"Threshold",threshDial);
    jassert(threshAttach != nullptr);
    makeupAttach = std::make_unique<Attachment>(audioProcessor.apvts,"Makeup",makeupDial);
    jassert(makeupAttach != nullptr);
    
    masterDial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    masterDial.setTextBoxStyle(juce::Slider::TextBoxBelow,false, 90, 0);
    masterDial.setTextValueSuffix(" %");
    masterDial.setColour(juce::Slider::ColourIds::rotarySliderFillColourId, mycolors.mydarkPink);
    masterDial.setColour(juce::Slider::ColourIds::rotarySliderOutlineColourId, mycolors.mymedPink);
    masterDial.setColour(juce::Slider::ColourIds::thumbColourId, mycolors.mybrown);

    threshDial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    threshDial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, threshDial.getWidth(), -30);
    threshDial.setTextValueSuffix(" dB");
    threshDial.setColour(juce::Slider::ColourIds::rotarySliderFillColourId, mycolors.mydarkPink);
    threshDial.setColour(juce::Slider::ColourIds::rotarySliderOutlineColourId, mycolors.mymedPink);
    threshDial.setColour(juce::Slider::ColourIds::thumbColourId, mycolors.mybrown);
    
    makeupDial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    makeupDial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 0);
    makeupDial.setTextValueSuffix(" dB");
    makeupDial.setColour(juce::Slider::ColourIds::rotarySliderFillColourId, mycolors.mydarkPink);
    makeupDial.setColour(juce::Slider::ColourIds::rotarySliderOutlineColourId, mycolors.mymedPink);
    makeupDial.setColour(juce::Slider::ColourIds::thumbColourId, mycolors.mybrown);
}

CompressorPieceAudioProcessorEditor::~CompressorPieceAudioProcessorEditor()
{
}

//==============================================================================
void CompressorPieceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(mycolors.mylightPink);
    juce::Image background = juce::ImageCache::getFromMemory(BinaryData::makeup0_75x_png, BinaryData::makeup0_75x_pngSize);
    g.drawImageAt(background, 0, 0);
}

void CompressorPieceAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    analyzer.setBounds(bounds.removeFromTop(295));
    
    masterDial.setBounds(125, 540, 200, 125);
    threshDial.setBounds(90, 370, 100, 120);
    makeupDial.setBounds(265, 370, 100, 120);
}
