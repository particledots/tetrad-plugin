#include "PluginProcessor.h"
#include "PluginEditor.h"

TetradProcessor::TetradProcessor()
    : AudioProcessor (BusesProperties()),                       // a MIDI effect: no audio buses
      apvts (*this, nullptr, "Tetrad", TetradParams::createLayout())
{
    modeParam       = apvts.getRawParameterValue (TetradParams::mode);
    waveParam       = apvts.getRawParameterValue (TetradParams::asgnWave);
    tempoParam      = apvts.getRawParameterValue (TetradParams::tempoFollow);
    rectOutParam    = apvts.getRawParameterValue (TetradParams::rectOut);
    outChannelParam = apvts.getRawParameterValue (TetradParams::outChannel);
    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        sliderParam[c]    = apvts.getRawParameterValue (TetradParams::slider[ch]);
        modFromParam[c]   = apvts.getRawParameterValue (TetradParams::modFrom[ch]);
        modDepthParam[c]  = apvts.getRawParameterValue (TetradParams::modDepth[ch]);
        resetFromParam[c] = apvts.getRawParameterValue (TetradParams::resetFrom[ch]);
        sineOnParam[c]    = apvts.getRawParameterValue (TetradParams::sineOn[ch]);
        asgnOnParam[c]    = apvts.getRawParameterValue (TetradParams::asgnOn[ch]);
        rectOnParam[c]    = apvts.getRawParameterValue (TetradParams::rectOn[ch]);
        sineLevelParam[c]   = apvts.getRawParameterValue (TetradParams::sineLevel[ch]);
        asgnLevelParam[c]   = apvts.getRawParameterValue (TetradParams::asgnLevel[ch]);
        exWaveParam[c]      = apvts.getRawParameterValue (TetradParams::exWave[ch]);
        shapeAmountParam[c] = apvts.getRawParameterValue (TetradParams::shapeAmount[ch]);
        shapeFromParam[c]   = apvts.getRawParameterValue (TetradParams::shapeFrom[ch]);
        shapeTargetParam[c] = apvts.getRawParameterValue (TetradParams::shapeTarget[ch]);
    }
    exOnParam = apvts.getRawParameterValue (TetradParams::exOn);
    inputs.reserve (4096);
    outBuffer.ensureSize (65536);

    // A new copy gets its own random sequences.
    seed.store ((uint32_t) juce::Random::getSystemRandom().nextInt (0x7fffffff) + 1u);
    engine.setSeed (seed.load());
    engine.prepare (48000.0);
    readSettings();
    publishToPanel();
}

void TetradProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);            // the channels stay where they are
    outBuffer.ensureSize (65536);
}

bool TetradProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet().isDisabled() && layouts.getMainInputChannelSet().isDisabled();
}

void TetradProcessor::readSettings()
{
    tetrad::Settings s;
    s.mode        = juce::roundToInt (modeParam->load());
    s.asgnWave    = juce::roundToInt (waveParam->load());
    s.tempoFollow = tempoParam->load() >= 0.5f;
    s.rectOut     = juce::roundToInt (rectOutParam->load());
    for (int ch = 0; ch < 4; ++ch)
    {
        const auto c = (size_t) ch;
        s.slider[ch]    = sliderParam[c]->load();
        s.modFrom[ch]   = juce::roundToInt (modFromParam[c]->load());
        s.modDepth[ch]  = modDepthParam[c]->load();
        s.resetFrom[ch] = juce::roundToInt (resetFromParam[c]->load());
        s.sineOn[ch]    = sineOnParam[c]->load() >= 0.5f;
        s.asgnOn[ch]    = asgnOnParam[c]->load() >= 0.5f;
        s.rectOn[ch]    = rectOnParam[c]->load() >= 0.5f;
        s.sineLevel[ch]   = sineLevelParam[c]->load();
        s.asgnLevel[ch]   = asgnLevelParam[c]->load();
        s.exWave[ch]      = juce::roundToInt (exWaveParam[c]->load());
        s.shapeAmount[ch] = shapeAmountParam[c]->load();
        s.shapeFrom[ch]   = juce::roundToInt (shapeFromParam[c]->load());
        s.shapeTarget[ch] = juce::roundToInt (shapeTargetParam[c]->load());
    }
    s.exOn = exOnParam->load() >= 0.5f;
    engine.setSettings (s);
}

tetrad::HostTime TetradProcessor::readHostTime()
{
    tetrad::HostTime h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { h.hasTempo = *bpm > 1.0; h.bpm = *bpm; }
            if (auto ppq = pos->getPpqPosition()) { h.hasPosition = true; h.ppq = *ppq; }
            h.playing = pos->getIsPlaying();
        }
    return h;
}

void TetradProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    if (engine.getSeed() != seed.load()) engine.setSeed (seed.load());    // a project's seed, loaded on the message thread
    readSettings();
    engine.setHostTime (readHostTime());

    // Everything that comes in goes out unchanged; the mod wheel and note-ons are also read.
    inputs.clear();
    outBuffer.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int offset = juce::jlimit (0, numSamples - 1, meta.samplePosition);
        outBuffer.addEvent (m, offset);
        if (inputs.size() >= inputs.capacity()) continue;
        if (m.isController() && m.getControllerNumber() == 1)
            inputs.push_back ({ offset, tetrad::InputEvent::modWheel, m.getControllerValue() / 127.0 });
        else if (m.isNoteOn())
            inputs.push_back ({ offset, tetrad::InputEvent::noteOn, 0.0 });
    }

    sink.out = &outBuffer;
    const int channel = juce::jlimit (1, 16, juce::roundToInt (outChannelParam->load()));
    if (lastChannel > 0 && channel != lastChannel)
    {
        // A new MIDI out channel: end what is on (rect CC at 127, a rect note) on the old one, then start afresh.
        sink.channel = lastChannel;
        engine.closeAll (0, sink);
    }
    lastChannel = channel;
    sink.channel = channel;
    engine.render (numSamples, inputs.data(), (int) inputs.size(), sink);
    midi.swapWith (outBuffer);
    publishToPanel();

    // About every 20 ms, a copy of the traces for the Waves view.
    samplesSinceTrace += numSamples;
    if (samplesSinceTrace < (int) (getSampleRate() > 0 ? getSampleRate() * 0.02 : 960.0))
        return;
    const juce::SpinLock::ScopedTryLockType l (traceLock);
    if (l.isLocked())
    {
        samplesSinceTrace = 0;
        for (int ch = 0; ch < tetrad::kNumChannels; ++ch)
            for (int k = 0; k < tetrad::Engine::kTracePoints; ++k)
            {
                traces[(size_t) ch][(size_t) (k * 2)]     = engine.trace (ch, 0, k);
                traces[(size_t) ch][(size_t) (k * 2 + 1)] = engine.trace (ch, 1, k);
            }
        tracePoints = tetrad::Engine::kTracePoints;
    }
}

void TetradProcessor::processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    // Bypassed: incoming MIDI passes untouched and nothing new is added, except the end of a rect note or gate that was on.
    sink.out = &midi;
    sink.channel = lastChannel > 0 ? lastChannel : juce::jlimit (1, 16, juce::roundToInt (outChannelParam->load()));
    engine.closeAll (0, sink);
}

bool TetradProcessor::copyTraces (Traces& dest)
{
    const juce::SpinLock::ScopedTryLockType l (traceLock);
    if (! l.isLocked() || tracePoints == 0) return false;
    dest = traces;
    return true;
}

void TetradProcessor::publishToPanel()
{
    const auto mode = (tetrad::Mode) engine.getSettings().mode;
    for (int ch = 0; ch < tetrad::kNumChannels; ++ch)
    {
        auto& v = view[(size_t) ch];
        v.sine.store ((float) engine.sine (ch));
        v.asgn.store ((float) engine.asgn (ch));
        v.hz.store ((float) engine.hz (ch));
        v.rect.store (engine.rect (ch));
        v.note.store (engine.noteOn (ch));
        v.ratio.store (engine.ratioUsed (ch));
        v.length.store (engine.lengthUsed (ch));
        v.shape.store ((float) engine.shape (ch));
        // For the mode light, which blinks as a slider crosses 90, 180 or 270 degrees (phase mode) or moves to another
        // ratio (divide, multiply), to help set it, as on the module. Only the slider counts, not "mod from".
        const double x = engine.getSettings().slider[ch];
        int marker = -1;
        if (ch > 0 && mode == tetrad::Mode::phase)
            marker = juce::jlimit (0, 3, (int) std::floor (x * 4.0));
        else if (ch > 0 && (mode == tetrad::Mode::divide || mode == tetrad::Mode::multiply))
            marker = 1000 + tetrad::sliderRatioIndex (x);
        v.marker.store (marker);
    }
    tempoMissing.store (engine.tempoMissing());
    blocksDone.fetch_add (1);
}

void TetradProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (seedProperty, (int) seed.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TetradProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const int saved = (int) tree.getProperty (seedProperty, 0);
            tree.removeProperty (seedProperty, nullptr);
            apvts.replaceState (tree);
            if (saved > 0)
                seed.store ((uint32_t) saved);             // the audio thread picks it up at its next block
        }
}

juce::AudioProcessorEditor* TetradProcessor::createEditor()
{
    return new TetradEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TetradProcessor();
}
