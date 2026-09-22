// Dychka time & volume envelope effect - Copyright (C) 2026 Andrii Dalevskyi (Dallas Audio)
// SPDX-License-Identifier: AGPL-3.0-or-later
// This file is part of Dychka, released under the GNU AGPL v3.0 or later; see LICENSE.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Model/FactoryEnvelopes.h"
#include "Model/BankStore.h"

using namespace dychka;

DychkaAudioProcessor::BusesProperties DychkaAudioProcessor::makeBusesProperties()
{
    return BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

//==============================================================================
DychkaAudioProcessor::DychkaAudioProcessor()
    : AudioProcessor (makeBusesProperties()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    raw.on        = apvts.getRawParameterValue (ParamIDs::on);
    raw.timeSlot  = apvts.getRawParameterValue (ParamIDs::timeSlot);
    raw.volSlot   = apvts.getRawParameterValue (ParamIDs::volSlot);
    raw.sync      = apvts.getRawParameterValue (ParamIDs::sync);
    raw.tempo     = apvts.getRawParameterValue (ParamIDs::tempo);
    raw.smoothing = apvts.getRawParameterValue (ParamIDs::smoothing);
    raw.attack    = apvts.getRawParameterValue (ParamIDs::attack);
    raw.release   = apvts.getRawParameterValue (ParamIDs::release);
    raw.mix       = apvts.getRawParameterValue (ParamIDs::mix);
    raw.output    = apvts.getRawParameterValue (ParamIDs::output);
    raw.midiTime  = apvts.getRawParameterValue (ParamIDs::midiTime);
    raw.midiVol   = apvts.getRawParameterValue (ParamIDs::midiVol);
    raw.timeNote  = apvts.getRawParameterValue (ParamIDs::timeNote);
    raw.volNote   = apvts.getRawParameterValue (ParamIDs::volNote);

    bank = factoryBank();
    bank.name = "Factory";
    publishAll();
    resetUndoHistory();

    apvts.addParameterListener (ParamIDs::timeSlot, this);
    apvts.addParameterListener (ParamIDs::volSlot, this);
    startTimerHz (30);
}

DychkaAudioProcessor::~DychkaAudioProcessor()
{
    stopTimer();
    apvts.removeParameterListener (ParamIDs::timeSlot, this);
    apvts.removeParameterListener (ParamIDs::volSlot, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout DychkaAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    using Bool = juce::AudioParameterBool;
    using Int = juce::AudioParameterInt;
    using Choice = juce::AudioParameterChoice;
    using Attr = juce::AudioParameterFloatAttributes;
    using IntAttr = juce::AudioParameterIntAttributes;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto pctText  = [] (float v, int) { return juce::String ((int) std::round (v)); };
    auto dbText   = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto msText   = [] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; };
    auto bpmText  = [] (float v, int) { return juce::String (v, 1) + " BPM"; };
    auto noteText = [] (int v, int) { return juce::MidiMessage::getMidiNoteName (v, true, true, 4) + " (" + juce::String (v) + ")"; };

    p.push_back (std::make_unique<Bool> (juce::ParameterID { ParamIDs::on, 1 }, "Effect On", true));
    p.push_back (std::make_unique<Int> (juce::ParameterID { ParamIDs::timeSlot, 1 }, "Time Slot", 1, kNumSlots, 1));
    p.push_back (std::make_unique<Int> (juce::ParameterID { ParamIDs::volSlot, 1 }, "Volume Slot", 1, kNumSlots, 1));

    juce::StringArray syncNames { "Host", "Internal" };
    p.push_back (std::make_unique<Choice> (juce::ParameterID { ParamIDs::sync, 1 }, "Sync", syncNames, 0));
    p.push_back (std::make_unique<Float> (juce::ParameterID { ParamIDs::tempo, 1 }, "Tempo",
                                          Range (40.0f, 300.0f, 0.1f, 0.6f), 120.0f, Attr().withLabel ("BPM").withStringFromValueFunction (bpmText)));
    p.push_back (std::make_unique<Float> (juce::ParameterID { ParamIDs::smoothing, 1 }, "Smoothing",
                                          Range (0.0f, 100.0f, 1.0f), 0.0f, Attr().withStringFromValueFunction (pctText)));
    p.push_back (std::make_unique<Float> (juce::ParameterID { ParamIDs::attack, 1 }, "Volume Attack",
                                          Range (0.0f, 200.0f, 0.1f, 0.4f), 1.0f, Attr().withLabel ("ms").withStringFromValueFunction (msText)));
    p.push_back (std::make_unique<Float> (juce::ParameterID { ParamIDs::release, 1 }, "Volume Release",
                                          Range (0.0f, 200.0f, 0.1f, 0.4f), 1.0f, Attr().withLabel ("ms").withStringFromValueFunction (msText)));
    p.push_back (std::make_unique<Float> (juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
                                          Range (0.0f, 100.0f, 1.0f), 100.0f, Attr().withStringFromValueFunction (pctText)));
    p.push_back (std::make_unique<Float> (juce::ParameterID { ParamIDs::output, 1 }, "Output",
                                          Range (-60.0f, 6.0f, 0.01f), 0.0f, Attr().withLabel ("dB").withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<Bool> (juce::ParameterID { ParamIDs::midiTime, 1 }, "MIDI Time Slots", true));
    p.push_back (std::make_unique<Bool> (juce::ParameterID { ParamIDs::midiVol, 1 }, "MIDI Volume Slots", true));
    p.push_back (std::make_unique<Int> (juce::ParameterID { ParamIDs::timeNote, 1 }, "Time Base Note", 0, 127 - kNumSlots + 1, 60,
                                        IntAttr().withStringFromValueFunction (noteText)));
    p.push_back (std::make_unique<Int> (juce::ParameterID { ParamIDs::volNote, 1 }, "Volume Base Note", 0, 127 - kNumSlots + 1, 24,
                                        IntAttr().withStringFromValueFunction (noteText)));

    return { p.begin(), p.end() };
}

//==============================================================================
const juce::String DychkaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

void DychkaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    engine.prepare (currentSampleRate, samplesPerBlock);
}

void DychkaAudioProcessor::releaseResources()
{
}

bool DychkaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (! in.isDisabled() && in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return true;
}

EngineParams DychkaAudioProcessor::readParams() const noexcept
{
    EngineParams p;
    p.slot[0] = juce::jlimit (0, kNumSlots - 1, (int) std::lround (raw.timeSlot->load()) - 1);
    p.slot[1] = juce::jlimit (0, kNumSlots - 1, (int) std::lround (raw.volSlot->load()) - 1);
    p.on = raw.on->load() >= 0.5f;
    p.sync = raw.sync->load() >= 0.5f ? SyncMode::internal : SyncMode::host;
    p.tempoBpm = raw.tempo->load();
    p.smoothing = raw.smoothing->load() * 0.01f;
    p.attackMs = raw.attack->load();
    p.releaseMs = raw.release->load();
    p.mix = raw.mix->load() * 0.01f;
    p.midiSlots[0] = raw.midiTime->load() >= 0.5f;
    p.midiSlots[1] = raw.midiVol->load() >= 0.5f;
    p.baseNote[0] = (int) std::lround (raw.timeNote->load());
    p.baseNote[1] = (int) std::lround (raw.volNote->load());
    return p;
}

HostPosition DychkaAudioProcessor::readHostPosition()
{
    HostPosition h;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            const auto bpm = pos->getBpm();
            const auto ppq = pos->getPpqPosition();
            if (bpm.hasValue() && ppq.hasValue() && *bpm > 0.0)
            {
                h.valid = true;
                h.playing = pos->getIsPlaying();
                h.bpm = *bpm;
                h.ppq = *ppq;
                if (auto ts = pos->getTimeSignature()) { h.tsNumerator = juce::jmax (1, ts->numerator); h.tsDenominator = juce::jmax (1, ts->denominator); }
                if (auto bar = pos->getPpqPositionOfLastBarStart()) h.barStartPpq = *bar;
                else
                {
                    const double quartersPerBar = h.tsNumerator * 4.0 / h.tsDenominator;
                    h.barStartPpq = std::floor (h.ppq / quartersPerBar) * quartersPerBar;
                }
            }
        }
    }
    lastHostValid.store (h.valid, std::memory_order_relaxed);
    lastHostPlaying.store (h.valid && h.playing, std::memory_order_relaxed);
    return h;
}

void DychkaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto params = readParams();
    const auto host = readHostPosition();

    if (params.on && ! prevOn)
        engine.requestRestart(); // switching on restarts the envelopes
    prevOn = params.on;

    engine.processBlock (buffer, getTotalNumInputChannels(), midiMessages, params, host);
    midiMessages.clear();

    const float outGain = juce::Decibels::decibelsToGain (raw.output->load(), -60.0f);
    buffer.applyGain (outGain);

    const int numSamples = buffer.getNumSamples();
    outputPeakL.store (buffer.getNumChannels() > 0 ? buffer.getMagnitude (0, 0, numSamples) : 0.0f, std::memory_order_relaxed);
    outputPeakR.store (buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, numSamples) : outputPeakL.load(), std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* DychkaAudioProcessor::createEditor()
{
    return new DychkaAudioProcessorEditor (*this);
}

//==============================================================================
void DychkaAudioProcessor::setEditKind (EnvKind kind)
{
    if (editKind == kind) return;
    editKind = kind;
    rebaseCommit (kind, getCurrentSlot (kind));
    sendChangeMessage();
}

int DychkaAudioProcessor::getCurrentSlot (EnvKind kind) const noexcept
{
    auto* r = kind == EnvKind::volume ? raw.volSlot : raw.timeSlot;
    return juce::jlimit (0, kNumSlots - 1, (int) std::lround (r->load()) - 1);
}

void DychkaAudioProcessor::setParam (const char* id, float plainValue)
{
    if (auto* param = apvts.getParameter (id))
    {
        const float norm = param->convertTo0to1 (plainValue);
        if (! juce::approximatelyEqual (param->getValue(), norm))
            param->setValueNotifyingHost (norm);
    }
}

void DychkaAudioProcessor::setCurrentSlot (EnvKind kind, int slot)
{
    setParam (ParamIDs::slot (kind), (float) (juce::jlimit (0, kNumSlots - 1, slot) + 1));
}

void DychkaAudioProcessor::parameterChanged (const juce::String&, float)
{
    // A slot parameter moved (UI, automation, MIDI echo): the engine follows it on the audio
    // thread by itself; the UI is refreshed from the timer (this may run on any thread).
    slotParamChanged.store (true, std::memory_order_release);
}

void DychkaAudioProcessor::rebaseCommit (EnvKind kind, int slot)
{
    committedKind = (int) kind;
    committedSlot = slot;
    committedJson = bank.of (kind)[(size_t) slot].toJsonString (kind);
    lastCommitMs = 0;
}

void DychkaAudioProcessor::commitCurrent (EnvKind kind)
{
    const int slot = getCurrentSlot (kind);
    const auto json = bank.of (kind)[(size_t) slot].toJsonString (kind);
    if (slot != committedSlot || (int) kind != committedKind)
    {
        // selection moved without an edit: just re-base
        rebaseCommit (kind, slot);
        return;
    }
    if (json == committedJson) return;

    const auto now = juce::Time::getMillisecondCounter();
    const bool merge = undoGestureDepth > 0 || (now - lastCommitMs) < kUndoMergeWindowMs;
    if (! merge)
    {
        undoStack.push_back ({ (int) kind, slot, committedJson });
        if (undoStack.size() > kMaxUndoSteps) undoStack.erase (undoStack.begin());
    }
    redoStack.clear();
    committedJson = json;
    lastCommitMs = now;
}

void DychkaAudioProcessor::envelopeEdited (EnvKind kind)
{
    const int slot = getCurrentSlot (kind);
    auto& e = bank.of (kind)[(size_t) slot];
    e.clampAll();
    if (committedSlot != slot || committedKind != (int) kind) { committedKind = (int) kind; committedSlot = slot; committedJson = {}; }
    commitCurrent (kind);
    engine.publishEnvelope (kind, slot, e);
    sendChangeMessage();
}

void DychkaAudioProcessor::replaceSlot (EnvKind kind, int slot, const Envelope& envelope)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots)) return;
    auto& target = bank.of (kind)[(size_t) slot];
    undoStack.push_back ({ (int) kind, slot, target.toJsonString (kind) });
    if (undoStack.size() > kMaxUndoSteps) undoStack.erase (undoStack.begin());
    redoStack.clear();
    target = envelope;
    target.clampAll();
    if (slot == getCurrentSlot (kind) && (int) kind == committedKind) rebaseCommit (kind, slot);
    engine.publishEnvelope (kind, slot, target);
    sendChangeMessage();
}

void DychkaAudioProcessor::resetSlotToFactory (EnvKind kind, int slot)
{
    replaceSlot (kind, slot, factoryEnvelope (kind, slot));
}

bool DychkaAudioProcessor::isSlotModified (EnvKind kind, int slot) const
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots)) return false;
    return bank.of (kind)[(size_t) slot] != factoryEnvelope (kind, slot);
}

void DychkaAudioProcessor::applyEntry (UndoEntry entry, std::vector<UndoEntry>& pushTo)
{
    if (! juce::isPositiveAndBelow (entry.slot, kNumSlots) || ! juce::isPositiveAndBelow (entry.kind, kNumKinds)) return;
    const auto kind = (EnvKind) entry.kind;
    auto& target = bank.of (kind)[(size_t) entry.slot];
    pushTo.push_back ({ entry.kind, entry.slot, target.toJsonString (kind) });
    Envelope restored;
    if (restored.fromJsonString (entry.json))
        target = std::move (restored);
    editKind = kind;
    rebaseCommit (kind, entry.slot);
    if (entry.slot != getCurrentSlot (kind)) setCurrentSlot (kind, entry.slot);
    engine.publishEnvelope (kind, entry.slot, target);
    sendChangeMessage();
}

void DychkaAudioProcessor::undo()
{
    if (undoStack.empty()) return;
    auto e = undoStack.back();
    undoStack.pop_back();
    applyEntry (e, redoStack);
}

void DychkaAudioProcessor::redo()
{
    if (redoStack.empty()) return;
    auto e = redoStack.back();
    redoStack.pop_back();
    applyEntry (e, undoStack);
}

void DychkaAudioProcessor::resetUndoHistory()
{
    undoStack.clear();
    redoStack.clear();
    rebaseCommit (editKind, getCurrentSlot (editKind));
}

void DychkaAudioProcessor::publishAll()
{
    engine.publishBank (bank);
    sendChangeMessage();
}

//==============================================================================
bool DychkaAudioProcessor::saveBankToFile (const juce::File& file)
{
    if (bank.name.isEmpty() || bank.name == "Factory")
        bank.name = file.getFileName().upToFirstOccurrenceOf (".", false, false);
    if (! BankStore::saveBank (bank, file)) return false;
    bankFile = file;
    sendChangeMessage();
    return true;
}

bool DychkaAudioProcessor::loadBankFromFile (const juce::File& file, juce::String* message)
{
    Bank loaded = bank;
    int n = 0;
    if (! BankStore::loadBank (file, loaded, &n))
    {
        if (message != nullptr) *message = "Not a Dychka bank file";
        return false;
    }
    bank = std::move (loaded);
    for (auto& kind : bank.slots) for (auto& s : kind) s.clampAll();
    bankFile = file;
    resetUndoHistory();
    publishAll();
    if (message != nullptr) *message = juce::String (n) + " slots loaded from " + file.getFileName();
    return true;
}

bool DychkaAudioProcessor::importFile (const juce::File& file, juce::String& message)
{
    const auto kind = BankStore::kindOfFile (file);

    if (kind == "envelope")
    {
        Envelope e;
        EnvKind k = editKind;
        if (! BankStore::loadEnvelope (file, e, &k)) { message = "Not a Dychka envelope file"; return false; }
        const int slot = getCurrentSlot (k);
        replaceSlot (k, slot, e);
        setEditKind (k);
        message = "Loaded " + juce::String (kindName (k)) + " envelope \"" + e.name + "\" into slot " + juce::String (kindLetter (k)) + juce::String (slot + 1).paddedLeft ('0', 2);
        return true;
    }

    if (kind == "bank")
        return loadBankFromFile (file, &message);

    message = "Unsupported file: " + file.getFileName() + " (use .dychka-envelope.json or .dychka-bank.json)";
    return false;
}

bool DychkaAudioProcessor::exportEnvelopeJson (EnvKind kind, int slot, const juce::File& file)
{
    return BankStore::saveEnvelope (getEnvelope (kind, slot), kind, file);
}

//==============================================================================
void DychkaAudioProcessor::tap()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (! tapTimesMs.empty() && now - tapTimesMs.back() > 2000)
        tapTimesMs.clear();
    if (tapTimesMs.empty())
        engine.requestRestart(); // the first tap cues the envelopes
    tapTimesMs.push_back (now);
    if (tapTimesMs.size() > 5) tapTimesMs.erase (tapTimesMs.begin());

    if (tapTimesMs.size() >= 2)
    {
        double mean = 0.0;
        for (size_t i = 1; i < tapTimesMs.size(); ++i) mean += (double) (tapTimesMs[i] - tapTimesMs[i - 1]);
        mean /= (double) (tapTimesMs.size() - 1);
        const float bpm = juce::jlimit (40.0f, 300.0f, (float) (60000.0 / juce::jmax (1.0, mean)));
        setParam (ParamIDs::tempo, bpm);
    }
}

void DychkaAudioProcessor::timerCallback()
{
    // Slot requests recorded by the engine (MIDI notes): the parameter follows the audio thread.
    for (int k = 0; k < kNumKinds; ++k)
        if (const int s = engine.pendingSlot[(size_t) k].exchange (-1); s >= 0)
            setCurrentSlot ((EnvKind) k, s);

    if (slotParamChanged.exchange (false))
    {
        // re-base the undo commit point on the new slot and refresh the panels
        rebaseCommit (editKind, getCurrentSlot (editKind));
        sendChangeMessage();
    }
}

//==============================================================================
void DychkaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("Dychka");

    if (auto paramsXml = apvts.copyState().createXml())
        root.addChildElement (paramsXml.release());

    // Only the slots that differ from the factory bank are stored.
    std::array<std::array<bool, kNumSlots>, kNumKinds> modified {};
    for (int k = 0; k < kNumKinds; ++k)
        for (int i = 0; i < kNumSlots; ++i)
            modified[(size_t) k][(size_t) i] = isSlotModified ((EnvKind) k, i);

    auto* bankXml = root.createNewChildElement ("bank");
    bankXml->setAttribute ("file", bankFile.getFullPathName());
    bankXml->setAttribute ("name", bank.name);
    bankXml->setAttribute ("edit", (int) editKind);
    bankXml->addTextElement (bank.toJsonString (&modified));

    copyXmlToBinary (root, destData);
}

void DychkaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> root (getXmlFromBinary (data, sizeInBytes));
    if (root == nullptr || ! root->hasTagName ("Dychka"))
        return;

    if (auto* paramsXml = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*paramsXml));

    if (auto* bankXml = root->getChildByName ("bank"))
    {
        Bank restored = factoryBank();
        restored.fromJsonString (bankXml->getAllSubText());
        restored.name = bankXml->getStringAttribute ("name", "Factory");
        for (auto& kind : restored.slots) for (auto& s : kind) s.clampAll();
        bank = std::move (restored);
        const auto path = bankXml->getStringAttribute ("file");
        bankFile = path.isNotEmpty() ? juce::File (path) : juce::File();
        editKind = (EnvKind) juce::jlimit (0, kNumKinds - 1, bankXml->getIntAttribute ("edit", 0));
    }

    resetUndoHistory();
    publishAll();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DychkaAudioProcessor();
}
