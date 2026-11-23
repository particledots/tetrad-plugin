// What Logic sees: a MIDI effect (AU type aumi) with no audio. Incoming MIDI passes through untouched; the twelve
// outputs are added as CC (and rect notes, if chosen) on the chosen channel (12 by default). The incoming mod wheel
// (CC 1) and note-ons are also read (for "mod from" and "reset from"), and Logic's tempo and song position (for
// following the tempo).
//
// Saved with the project besides the parameters: the random seed (property "randomSeed"), so the random waves repeat
// when the project reopens; a new copy of the plugin gets a new seed. Each project opens with every channel at phase 0,
// like the module switched on.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "tetrad/Engine.h"
#include "Parameters.h"

#include <array>
#include <atomic>
#include <vector>

class TetradProcessor : public juce::AudioProcessor
{
public:
    TetradProcessor();
    ~TetradProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;   // MIDI passes; a rect note or gate still on is ended

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tetrad"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    uint32_t randomSeed() const { return seed.load(); }

    // For the panel: the latest values (written by the audio thread after every block).
    struct ChannelView
    {
        std::atomic<float> sine { 0.0f }, asgn { 0.0f }, hz { 0.0f };
        std::atomic<bool> rect { false }, note { false };
        std::atomic<int> ratio { 1 }, length { 0 }, marker { -1 };   // marker: the slider's phase quarter or ratio, for the mode light
    };
    std::array<ChannelView, tetrad::kNumChannels> view {};
    std::atomic<bool> tempoMissing { false };
    std::atomic<int> blocksDone { 0 };

    // Copies the Waves-view traces (sine and asgn of each channel, oldest first) into dest; false if not available now.
    using Traces = std::array<std::array<float, tetrad::Engine::kTracePoints * 2>, tetrad::kNumChannels>;
    bool copyTraces (Traces& dest);

    static inline const juce::Identifier seedProperty { "randomSeed" };

private:
    struct Sink : tetrad::CcSink
    {
        juce::MidiBuffer* out = nullptr;
        int channel = tetrad::kDefaultChannel;
        void cc (int offset, int number, int value) override
        {
            out->addEvent (juce::MidiMessage::controllerEvent (channel, number, value), offset);
        }
        void note (int offset, int number, int velocity) override
        {
            if (velocity > 0) out->addEvent (juce::MidiMessage::noteOn (channel, number, (juce::uint8) velocity), offset);
            else out->addEvent (juce::MidiMessage::noteOff (channel, number), offset);
        }
    };
    void publishToPanel();
    void readSettings();
    tetrad::HostTime readHostTime();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* waveParam = nullptr;
    std::atomic<float>* tempoParam = nullptr;
    std::atomic<float>* rectOutParam = nullptr;
    std::atomic<float>* outChannelParam = nullptr;
    std::array<std::atomic<float>*, 4> sliderParam {}, modFromParam {}, modDepthParam {}, resetFromParam {}, sineOnParam {}, asgnOnParam {}, rectOnParam {};

    tetrad::Engine engine;
    std::vector<tetrad::InputEvent> inputs;
    juce::MidiBuffer outBuffer;
    Sink sink;
    int lastChannel = -1;
    std::atomic<uint32_t> seed { 1 };

    juce::SpinLock traceLock;
    Traces traces {};
    int tracePoints = 0, samplesSinceTrace = 1 << 30;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TetradProcessor)
};
