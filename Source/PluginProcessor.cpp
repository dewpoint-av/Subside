#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    std::vector<subside::CurvePoint> defaultAudioCurve()
    {
        return { { 0.0f, 0.0f }, { 0.06f, 1.0f }, { 0.30f, 0.85f }, { 1.0f, 0.0f } };
    }

    std::vector<subside::CurvePoint> defaultMidiCurve()
    {
        return { { 0.0f, 0.0f }, { 0.12f, 1.0f }, { 0.50f, 0.80f }, { 1.0f, 0.0f } };
    }

    juce::String curveToString (const std::vector<subside::CurvePoint>& pts)
    {
        juce::String s;
        for (const auto& p : pts)
            s += juce::String (p.x, 4) + "," + juce::String (p.y, 4) + ";";
        return s;
    }

    std::vector<subside::CurvePoint> curveFromString (const juce::String& s)
    {
        std::vector<subside::CurvePoint> pts;
        for (const auto& t : juce::StringArray::fromTokens (s, ";", ""))
        {
            if (t.isEmpty())
                continue;
            const auto xy = juce::StringArray::fromTokens (t, ",", "");
            if (xy.size() >= 2)
                pts.push_back ({ xy[0].getFloatValue(), xy[1].getFloatValue() });
        }
        return pts;
    }
}

SubsideAudioProcessor::SubsideAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true))
    , parameters (*this, nullptr, "Parameters",
        {
            std::make_unique<juce::AudioParameterFloat> ("threshold", "Threshold",
                0.0f, 1.0f, 0.10f),
            std::make_unique<juce::AudioParameterFloat> ("audioTime", "Audio Time",
                juce::NormalisableRange<float> (0.05f, 4.0f, 0.0f, 0.5f), 0.80f),
            std::make_unique<juce::AudioParameterFloat> ("midiTime", "MIDI Time",
                juce::NormalisableRange<float> (0.05f, 4.0f, 0.0f, 0.5f), 1.00f),
            std::make_unique<juce::AudioParameterFloat> ("depth", "Depth",
                0.0f, 1.0f, 0.90f),
            std::make_unique<juce::AudioParameterFloat> ("lowDepth", "Low",
                0.0f, 1.0f, 1.00f),
            std::make_unique<juce::AudioParameterFloat> ("midDepth", "Mid",
                0.0f, 1.0f, 0.80f),
            std::make_unique<juce::AudioParameterFloat> ("highDepth", "High",
                0.0f, 1.0f, 0.60f),
            std::make_unique<juce::AudioParameterFloat> ("lookahead", "Lookahead",
                0.0f, 0.02f, 0.005f),
            std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                0.0f, 1.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat> ("output", "Output",
                0.0f, 2.0f, 1.0f)
        })
{
    curvePoints[0] = defaultAudioCurve();
    curvePoints[1] = defaultMidiCurve();
    engine.setCurve (0, curvePoints[0]);
    engine.setCurve (1, curvePoints[1]);
    writeCurvesToState();
}

SubsideAudioProcessor::~SubsideAudioProcessor() = default;

const juce::String SubsideAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SubsideAudioProcessor::acceptsMidi() const  { return true; }
bool SubsideAudioProcessor::producesMidi() const { return false; }
bool SubsideAudioProcessor::isMidiEffect() const { return false; }

double SubsideAudioProcessor::getTailLengthSeconds() const { return 5.0; }

int SubsideAudioProcessor::getNumPrograms()    { return 1; }
int SubsideAudioProcessor::getCurrentProgram() { return 0; }

void SubsideAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }

const juce::String SubsideAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void SubsideAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void SubsideAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    lastLatencySamples = -1;
}

void SubsideAudioProcessor::releaseResources()
{
    engine.reset();
}

bool SubsideAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != juce::AudioChannelSet::mono()
        && mainIn != juce::AudioChannelSet::stereo())
        return false;

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != mainOut)
        return false;

    // Sidechain (input bus 1) may be disabled, mono or stereo.
    if (layouts.getChannelSet (true, 1).size() > 2)
        return false;

    return true;
}

void SubsideAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    auto inBuf  = getBusBuffer (buffer, true, 0);
    auto outBuf = getBusBuffer (buffer, false, 0);

    for (int ch = inBuf.getNumChannels(); ch < outBuf.getNumChannels(); ++ch)
        outBuf.clear (ch, 0, numSamples);

    if (inBuf.getNumChannels() == 0 || outBuf.getNumChannels() == 0)
        return;

    const float* inL = inBuf.getReadPointer (0);
    const float* inR = inBuf.getNumChannels() > 1 ? inBuf.getReadPointer (1) : inL;

    float* outL = outBuf.getWritePointer (0);
    float* outR = outBuf.getNumChannels() > 1 ? outBuf.getWritePointer (1) : outL;

    // Sidechain (input bus 1).
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (auto* scBus = getBus (true, 1))
    {
        if (scBus->isEnabled())
        {
            const auto scBuf = getBusBuffer (buffer, true, 1);
            scL = scBuf.getReadPointer (0);
            scR = scBuf.getNumChannels() > 1 ? scBuf.getReadPointer (1) : scL;
        }
    }

    // MIDI trigger: any note-on in this block.
    bool midiTrigger = false;
    for (const auto meta : midiMessages)
    {
        if (meta.getMessage().isNoteOn())
        {
            midiTrigger = true;
            break;
        }
    }

    const auto param = [&] (const char* id)
    {
        return parameters.getRawParameterValue (id)->load();
    };

    subside::Params p;
    p.threshold  = param ("threshold");
    p.audioTime  = param ("audioTime");
    p.midiTime   = param ("midiTime");
    p.depth      = param ("depth");
    p.lowDepth   = param ("lowDepth");
    p.midDepth   = param ("midDepth");
    p.highDepth  = param ("highDepth");
    p.lookahead  = param ("lookahead");
    p.mix        = param ("mix");
    p.output     = param ("output");

    engine.setParams (p);

    // Report lookahead latency (updates only when it changes).
    const int lat = juce::roundToInt (p.lookahead * static_cast<float> (getSampleRate()));
    if (lat != lastLatencySamples)
    {
        lastLatencySamples = lat;
        setLatencySamples (lat);
    }

    engine.process (inL, inR, scL, scR, outL, outR, midiTrigger, numSamples);
}

bool SubsideAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SubsideAudioProcessor::createEditor()
{
    return new SubsideAudioProcessorEditor (*this);
}

void SubsideAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SubsideAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));

    readCurvesFromState();
}

std::vector<subside::CurvePoint> SubsideAudioProcessor::getCurvePoints (int lane) const
{
    if (lane < 0 || lane > 1)
        return {};
    return curvePoints[static_cast<size_t> (lane)];
}

void SubsideAudioProcessor::setCurvePoints (int lane, const std::vector<subside::CurvePoint>& points)
{
    if (lane < 0 || lane > 1)
        return;
    curvePoints[static_cast<size_t> (lane)] = points;
    engine.setCurve (lane, points);
    writeCurvesToState();
}

void SubsideAudioProcessor::writeCurvesToState()
{
    auto curves = parameters.state.getOrCreateChildWithName ("CURVES", nullptr);
    curves.setProperty ("audio", curveToString (curvePoints[0]), nullptr);
    curves.setProperty ("midi",  curveToString (curvePoints[1]), nullptr);
}

void SubsideAudioProcessor::readCurvesFromState()
{
    auto curves = parameters.state.getChildWithName ("CURVES");
    if (curves.isValid())
    {
        if (curves.hasProperty ("audio"))
            curvePoints[0] = curveFromString (curves.getProperty ("audio").toString());
        if (curves.hasProperty ("midi"))
            curvePoints[1] = curveFromString (curves.getProperty ("midi").toString());
    }
    engine.setCurve (0, curvePoints[0]);
    engine.setCurve (1, curvePoints[1]);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubsideAudioProcessor();
}
