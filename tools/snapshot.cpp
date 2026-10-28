// GUI-Snapshot: verarbeitet ein WAV durch den Processor (ohne Host) und rendert den Editor als PNG.
//   finisher-snapshot <in.wav> <out.png> [sekunden]

#include <cstdlib>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    if (argc < 3) { std::cerr << "usage: snapshot in.wav out.png [seconds]\n"; return 1; }
    juce::ScopedJuceInitialiser_GUI init;

    juce::AudioFormatManager afm;
    afm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (afm.createReaderFor (juce::File (argv[1])));
    if (! reader) return 1;

    FinisherAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, reader->sampleRate, 512);
    proc.prepareToPlay (reader->sampleRate, 512);
    if (auto* p = proc.apvts.getParameter (ParamIDs::drive)) p->setValueNotifyingHost (p->convertTo0to1 (2.5f));
    if (auto* p = proc.apvts.getParameter (ParamIDs::lsGain)) p->setValueNotifyingHost (p->convertTo0to1 (1.5f));
    if (auto* p = proc.apvts.getParameter (ParamIDs::hsGain)) p->setValueNotifyingHost (p->convertTo0to1 (1.0f));
    if (auto* p = proc.apvts.getParameter (ParamIDs::p1Gain)) p->setValueNotifyingHost (p->convertTo0to1 (-2.0f));
    // Alle Module an, ein paar typische Einstellungen
    auto set = [&proc] (const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    for (auto* id : { ParamIDs::trOn, ParamIDs::compOn, ParamIDs::mbOn, ParamIDs::satOn, ParamIDs::clipOn }) set (id, 1.0f);
    set (ParamIDs::p2Rng, -4.0f); set (ParamIDs::p2Thr, -35.0f); set (ParamIDs::p2Freq, 3500.0f);
    set (ParamIDs::hsMode, 2.0f); set (ParamIDs::hsGain, 2.0f);
    set (ParamIDs::trAtk, 3.0f); set (ParamIDs::satDrive, 4.0f); set (ParamIDs::satModel, 1.0f);
    set (ParamIDs::widthHi, 120.0f);

    const double secs = argc > 3 ? std::atof (argv[3]) : 1e9;
    const juce::int64 total = juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * secs));
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (juce::int64 pos = 0; pos < total; pos += 512)
    {
        reader->read (&buf, 0, 512, pos, true, true);
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* fe = dynamic_cast<FinisherAudioProcessorEditor*> (ed.get());
    for (int m = 0; m < FinisherAudioProcessorEditor::kNumModules; ++m)
    {
        fe->selectModule (m);
        for (int i = 0; i < 6; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (juce::String (argv[2]) + "_" + juce::String (m) + ".png");
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    std::cout << "I " << proc.engine.integrated.load() << " LRA " << proc.engine.lra.load() << " TP " << proc.engine.truePeakMax.load() << std::endl;
    return 0;
}
