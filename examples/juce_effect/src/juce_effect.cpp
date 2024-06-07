/*
@brief JUCE effect for Stratus example
@author Daniel Leonov
@date June 6, 2024
@brief A simple one-knob low-pass filter to showcase use of JUCE classes on Stratus
*/

#include "JuceHeader.h"
#include "dsp.hpp"

class effect : public dsp {
public:
    effect() {
        setName ("JUCE Effect");
        prepareToPlay (samplesPerBlockDefault);  // Expected block size on Stratus
        cutoffFrequencyChanged();  // Inits the LPF
    }

    void instanceConstants() override {
        version = STRATUS_EFFECT_VERSION;  // Set in your CMakeLists.txt
    }

    void compute (int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) override
    {
        // Unlikely to happen, but just in case
        if (count != samplesPerBlockPrevious)
            prepareToPlay (count);

        // There's no knob change callback, so we just poll it
        if (! juce::approximatelyEqual (lpfFreqKnob0To10Previous, lpfFreqKnob0To10))
            cutoffFrequencyChanged();

        // Required for JUCE DSP classes
        const juce::dsp::AudioBlock<FAUSTFLOAT> inputBlock (&inputs, (size_t)1, (size_t)count);
        juce::dsp::AudioBlock<FAUSTFLOAT> outputBlock (&outputs, (size_t)1, (size_t)count);
        juce::dsp::ProcessContextNonReplacing<FAUSTFLOAT> processContext (inputBlock, outputBlock);

        // Apply the filter
        lpf.process (processContext);
    }

private:
    // Controls
    float& lpfFreqKnob0To10 = knobs[0];

    // Expected audio process params
    const int samplesPerBlockDefault = 16;  // Most likely won't change
    const double sampleRateHz = 44100.0;  // Always

    // LPF
    juce::dsp::IIR::Filter<FAUSTFLOAT> lpf;
    const FAUSTFLOAT lpfQ = 0.666667f;  // 2 octaves

    // State change detectors
    int samplesPerBlockPrevious = samplesPerBlockDefault;
    float lpfFreqKnob0To10Previous = lpfFreqKnob0To10;

    void prepareToPlay (const int samplesPerBlock)
    {
        // This is what replaces JUCE's prepareToPlay().
        // Block size and sample rate aren't expected to change,
        // so it will only be called once.
        // But just in case, we still check the block size.

        juce::dsp::ProcessSpec processSpec = { sampleRateHz, (juce::uint32)samplesPerBlock, (juce::uint32)1 };
        lpf.prepare (processSpec);
        samplesPerBlockPrevious = samplesPerBlock;
    }

    void cutoffFrequencyChanged()
    {
        lpf.coefficients = juce::dsp::IIR::Coefficients<FAUSTFLOAT>::makeLowPass (
            sampleRateHz,
            getCutoffFrequencyHz (lpfFreqKnob0To10),
            lpfQ
            );
        lpfFreqKnob0To10Previous = lpfFreqKnob0To10;
    }

    inline FAUSTFLOAT getCutoffFrequencyHz (const float knobValue0To10)
    {
        return juce::jmap (
            knobValue0To10,
            0.0f,  // Knob min
            10.0f,  // Knob max
            20.f,  // Freq min
            5000.0f // Freq max
            );
    }
};

extern "C" {
    dsp * create() {
        return new effect;
    }
}
