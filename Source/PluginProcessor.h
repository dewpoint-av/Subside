#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/DuckEngine.h"

class SubsideAudioProcessor : public juce::AudioProcessor
{
public:
    SubsideAudioProcessor();
    ~SubsideAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    subside::DuckEngine& getEngine() { return engine; }

    // Curve access (message thread). Lane 0 = audio, 1 = MIDI.
    std::vector<subside::CurvePoint> getCurvePoints (int lane) const;
    void setCurvePoints (int lane, const std::vector<subside::CurvePoint>& points);

private:
    void writeCurvesToState();
    void readCurvesFromState();

    juce::AudioProcessorValueTreeState parameters;
    subside::DuckEngine engine;

    std::array<std::vector<subside::CurvePoint>, 2> curvePoints;
    int lastLatencySamples = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubsideAudioProcessor)
};
