// Host-Test: lädt Finisher.vst3, prüft Latenz, Verarbeitung, State-Roundtrip und rendert die GUI als PNG.
//   finisher-hosttest <Pfad/Finisher.vst3> <in.wav> <screenshot.png>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

static int fail (const juce::String& m) { std::cerr << "FAIL: " << m << std::endl; return 1; }

int main (int argc, char** argv)
{
    if (argc < 4) return fail ("usage: hosttest plugin.vst3 in.wav shot.png");
    juce::ScopedJuceInitialiser_GUI init;

    juce::AudioPluginFormatManager fm;
    fm.addDefaultFormats();
    juce::OwnedArray<juce::PluginDescription> descs;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (descs, argv[1]);
    if (descs.isEmpty()) return fail ("no plugin found");
    std::cout << "Plugin: " << descs[0]->name << " / " << descs[0]->manufacturerName << " / " << descs[0]->category << std::endl;

    juce::String err;
    auto inst = fm.createPluginInstance (*descs[0], 48000.0, 512, err);
    if (! inst) return fail ("instantiate: " + err);

    juce::AudioFormatManager afm;
    afm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (afm.createReaderFor (juce::File (argv[2])));
    if (! reader) return fail ("read wav");

    inst->setPlayConfigDetails (2, 2, reader->sampleRate, 512);
    inst->prepareToPlay (reader->sampleRate, 512);
    std::cout << "Latenz: " << inst->getLatencySamples() << " Samples" << std::endl;

    // Parameter per Host setzen (Drive +3 dB, Ceiling -1)
    for (auto* p : inst->getParameters())
    {
        const auto name = p->getName (64);
        if (name == "Drive")   p->setValueNotifyingHost (3.0f / 24.0f);
        if (name == "Ceiling") p->setValueNotifyingHost (5.0f / 6.0f);
    }
    std::cout << "Parameter: " << inst->getParameters().size() << std::endl;

    // 60 s verarbeiten, dabei Peak messen und auf NaN prüfen
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    const juce::int64 total = juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60));
    for (juce::int64 pos = 0; pos < total; pos += 512)
    {
        reader->read (&buf, 0, 512, pos, true, true);
        inst->processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
        {
            auto* d = buf.getReadPointer (c);
            for (int i = 0; i < 512; ++i)
            {
                if (! std::isfinite (d[i])) return fail ("NaN/Inf");
                peak = juce::jmax (peak, std::abs (d[i]));
            }
        }
    }
    std::cout << "Sample-Peak nach 60 s: " << juce::Decibels::gainToDecibels (peak) << " dBFS" << std::endl;

    // Variable Blockgrößen
    for (int bs : { 1, 7, 64, 333, 2048 })
    {
        juce::AudioBuffer<float> b (2, bs);
        reader->read (&b, 0, bs, 0, true, true);
        inst->prepareToPlay (reader->sampleRate, juce::jmax (bs, 512));
        inst->processBlock (b, midi);
    }
    std::cout << "Variable Blockgroessen OK" << std::endl;

    // State-Roundtrip
    juce::MemoryBlock state;
    inst->getStateInformation (state);
    auto inst2 = fm.createPluginInstance (*descs[0], 48000.0, 512, err);
    inst2->setStateInformation (state.getData(), (int) state.getSize());
    bool same = true;
    for (int i = 0; i < inst->getParameters().size(); ++i)
        if (std::abs (inst->getParameters()[i]->getValue() - inst2->getParameters()[i]->getValue()) > 1e-5f) same = false;
    std::cout << "State-Roundtrip: " << (same ? "OK" : "FEHLER") << " (" << state.getSize() << " Bytes)" << std::endl;
    if (! same) return fail ("state");

    // Editor rendern (nach etwas Verarbeitung, damit Meter Werte haben)
    std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
    if (! ed) return fail ("editor");
    ed->setVisible (true);
    // Timer laufen lassen
    for (int i = 0; i < 20; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argv[3]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::cout << "Screenshot: " << ed->getWidth() << "x" << ed->getHeight() << std::endl;

    ed.reset();
    inst->releaseResources();
    std::cout << "ALLE TESTS OK" << std::endl;
    return 0;
}
