#include "PluginEditor.h"

#include "DuckVisualizer.h"
#include "HoloLogo.h"

#include <cmath>
#include <functional>

namespace
{
    juce::String pctText  (float v) { return juce::String (v * 100.0f, 0) + " %"; }
    juce::String secText  (float v) { return juce::String (v, 2)  + " s"; }
    juce::String msText   (float v) { return juce::String (v * 1000.0f, 0) + " ms"; }
    juce::String gainText (float v) { return juce::String (v, 2)  + "x"; }

    // ---- a labelled rotary knob bound to a parameter -----------------------
    class Knob : public juce::Component,
                 private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& state,
              const juce::String& paramId,
              const juce::String& name,
              std::function<juce::String (float)> textFn)
            : stateRef (state), param (paramId), textFn (textFn)
        {
            slider.setSliderStyle (juce::Slider::Rotary);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setRotaryParameters ({ juce::MathConstants<float>::pi * 1.25f,
                                          juce::MathConstants<float>::pi * 2.75f,
                                          true });
            addAndMakeVisible (slider);

            nameLabel.setText (name, juce::dontSendNotification);
            nameLabel.setFont (juce::Font (11.0f, juce::Font::plain));
            nameLabel.setColour (juce::Label::textColourId, SympathyLookAndFeel::muted);
            nameLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (nameLabel);

            valueLabel.setFont (juce::Font (11.0f, juce::Font::bold));
            valueLabel.setColour (juce::Label::textColourId, SympathyLookAndFeel::text);
            valueLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (valueLabel);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                state, paramId, slider);

            stateRef.addParameterListener (param, this);
            refreshLabel();
        }

        ~Knob() override { stateRef.removeParameterListener (param, this); }

        void parameterChanged (const juce::String&, float) override { refreshLabel(); }

        void resized() override
        {
            auto b = getLocalBounds();
            nameLabel.setBounds (b.removeFromTop (18));
            valueLabel.setBounds (b.removeFromBottom (18));
            slider.setBounds (b);
        }

    private:
        void refreshLabel()
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (stateRef.getParameter (param)))
                if (textFn)
                    valueLabel.setText (textFn (p->getNormalisableRange().convertFrom0to1 (p->getValue())),
                                        juce::dontSendNotification);
        }

        juce::AudioProcessorValueTreeState& stateRef;
        juce::String param;
        std::function<juce::String (float)> textFn;
        juce::Slider slider;
        juce::Label nameLabel;
        juce::Label valueLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // ---- a titled panel that draws its own background ----------------------
    class Panel : public juce::Component
    {
    public:
        explicit Panel (const juce::String& title)
        {
            titleLabel.setText (title, juce::dontSendNotification);
            titleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
            titleLabel.setColour (juce::Label::textColourId, SympathyLookAndFeel::muted);
            titleLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (titleLabel);
        }

        void paint (juce::Graphics& g) override
        {
            SympathyLookAndFeel::drawPanelBackground (g, getLocalBounds());
        }

        void resized() override
        {
            titleLabel.setBounds (0, 8, getWidth(), 18);
        }

        juce::Label titleLabel;
    };

    void layoutKnobs (std::vector<std::unique_ptr<Knob>>& knobs, int cols,
                      juce::Rectangle<int> area, int gapX, int gapY)
    {
        if (knobs.empty())
            return;

        const int rows = static_cast<int> (std::ceil (knobs.size() / static_cast<double> (cols)));
        const int cw = (area.getWidth()  - gapX * (cols - 1)) / cols;
        const int ch = (area.getHeight() - gapY * (rows - 1)) / rows;
        const int knobH = juce::jmin (ch, 96);

        int idx = 0;
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols && idx < static_cast<int> (knobs.size()); ++c, ++idx)
            {
                const int x = area.getX() + c * (cw + gapX);
                const int y = area.getY() + r * (ch + gapY);
                knobs[static_cast<size_t> (idx)]->setBounds (x, y + (ch - knobH) / 2, cw, knobH);
            }
        }
    }
}

// ===========================================================================
class SubsideAudioProcessorEditor::Impl
{
public:
    explicit Impl (SubsideAudioProcessor& proc)
        : processor (proc)
    {
        build();
    }

    SubsideAudioProcessor& processor;

    std::unique_ptr<HoloTitle> title;
    juce::Label subtitle;
    juce::Label brand;
    juce::Label footer;

    std::unique_ptr<MainVisualizer> mainVis;
    std::unique_ptr<LanePanel> audioLane;
    std::unique_ptr<LanePanel> midiLane;

    std::unique_ptr<Panel> triggerPanel;
    std::unique_ptr<Panel> bandsPanel;
    std::unique_ptr<Panel> globalPanel;

    std::vector<std::unique_ptr<Knob>> triggerKnobs;
    std::vector<std::unique_ptr<Knob>> bandKnobs;
    std::vector<std::unique_ptr<Knob>> globalKnobs;

    void build()
    {
        title = std::make_unique<HoloTitle> ("SUBSIDE", 42.0f, true);

        subtitle.setText ("SIDECHAIN DUCKER", juce::dontSendNotification);
        subtitle.setFont (juce::Font (12.0f, juce::Font::bold));
        subtitle.setColour (juce::Label::textColourId, SympathyLookAndFeel::muted);
        subtitle.setJustificationType (juce::Justification::centred);

        brand.setText ("DEWPOINT AV", juce::dontSendNotification);
        brand.setFont (monumentFont (13.0f, false));
        brand.setColour (juce::Label::textColourId, SympathyLookAndFeel::accent);
        brand.setJustificationType (juce::Justification::centred);

        footer.setText (juce::String ("DEWPOINT AV  -  v") + juce::String (JucePlugin_VersionString), juce::dontSendNotification);
        footer.setFont (monumentFont (11.0f, false));
        footer.setColour (juce::Label::textColourId, SympathyLookAndFeel::muted);
        footer.setJustificationType (juce::Justification::centred);

        mainVis   = std::make_unique<MainVisualizer> (processor);
        audioLane = std::make_unique<LanePanel> (processor, 0);
        midiLane  = std::make_unique<LanePanel> (processor, 1);

        triggerPanel = std::make_unique<Panel> ("TRIGGER");
        triggerKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "threshold", "THRESHOLD", pctText));
        triggerKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "audioTime", "AUDIO TIME", secText));
        triggerKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "midiTime", "MIDI TIME", secText));

        bandsPanel = std::make_unique<Panel> ("DUCK BANDS");
        bandKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "lowDepth", "LOW", pctText));
        bandKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "midDepth", "MID", pctText));
        bandKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "highDepth", "HIGH", pctText));

        globalPanel = std::make_unique<Panel> ("GLOBAL");
        globalKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "depth", "DEPTH", pctText));
        globalKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "lookahead", "LOOKAHEAD", msText));
        globalKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "mix", "MIX", pctText));
        globalKnobs.push_back (std::make_unique<Knob> (processor.getParameters(), "output", "OUTPUT", gainText));
    }

    void attachTo (juce::Component& root)
    {
        root.addAndMakeVisible (*title);
        root.addAndMakeVisible (subtitle);
        root.addAndMakeVisible (brand);
        root.addAndMakeVisible (footer);

        root.addAndMakeVisible (*mainVis);
        root.addAndMakeVisible (*audioLane);
        root.addAndMakeVisible (*midiLane);

        root.addAndMakeVisible (*triggerPanel);
        for (auto& k : triggerKnobs) triggerPanel->addAndMakeVisible (*k);

        root.addAndMakeVisible (*bandsPanel);
        for (auto& k : bandKnobs) bandsPanel->addAndMakeVisible (*k);

        root.addAndMakeVisible (*globalPanel);
        for (auto& k : globalKnobs) globalPanel->addAndMakeVisible (*k);
    }

    void resized (juce::Rectangle<int> area)
    {
        auto b = area;

        // Footer strip.
        footer.setBounds (b.removeFromBottom (18));
        b.removeFromBottom (2);

        auto header = b.removeFromTop (92);
        brand.setBounds (header.removeFromTop (20));
        title->setBounds (header.removeFromTop (54));
        subtitle.setBounds (header);

        b.removeFromTop (6);
        mainVis->setBounds (b.removeFromTop (180));
        b.removeFromTop (10);

        // Two lane panels side by side.
        auto lanes = b.removeFromTop (170);
        const int gap = 12;
        audioLane->setBounds (lanes.removeFromLeft ((lanes.getWidth() - gap) / 2));
        lanes.removeFromLeft (gap);
        midiLane->setBounds (lanes);

        b.removeFromTop (10);

        // Three knob panels across the bottom.
        const int panelW = (b.getWidth() - 2 * gap) / 3;
        triggerPanel->setBounds (b.removeFromLeft (panelW));
        b.removeFromLeft (gap);
        bandsPanel->setBounds (b.removeFromLeft (panelW));
        b.removeFromLeft (gap);
        globalPanel->setBounds (b);

        auto tb = triggerPanel->getLocalBounds().reduced (12);
        tb.removeFromTop (30);
        layoutKnobs (triggerKnobs, 3, tb, 8, 0);

        auto bb = bandsPanel->getLocalBounds().reduced (12);
        bb.removeFromTop (30);
        layoutKnobs (bandKnobs, 3, bb, 8, 0);

        auto gb = globalPanel->getLocalBounds().reduced (12);
        gb.removeFromTop (30);
        layoutKnobs (globalKnobs, 4, gb, 6, 0);
    }
};

// ===========================================================================
SubsideAudioProcessorEditor::SubsideAudioProcessorEditor (SubsideAudioProcessor& p)
    : AudioProcessorEditor (&p)
    , processor (p)
{
    setLookAndFeel (&lookAndFeel);

    impl = std::make_unique<Impl> (processor);
    impl->attachTo (*this);

    setResizable (true, true);
    setResizeLimits (720, 640, 1500, 1000);
    setSize (940, 660);
}

SubsideAudioProcessorEditor::~SubsideAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SubsideAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (
        SympathyLookAndFeel::background.brighter (0.05f), b.getCentreX(), b.getCentreY(),
        SympathyLookAndFeel::background, b.getWidth() * 0.65f, b.getHeight() * 0.65f, true));
    g.fillAll();
}

void SubsideAudioProcessorEditor::resized()
{
    impl->resized (getLocalBounds().reduced (16));
}
