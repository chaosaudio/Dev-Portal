/*
 * AIDA-X Eq2Bands base class for Chaos Audio
 * Copyright (C) 2024 Massimo Pennazio <maxipenna@libero.it>
 * SPDX-License-Identifier: GPL-3.0-or-later
*/

#ifndef EQ2BANDS_HPP
#define EQ2BANDS_HPP

#include <cstdint>

#include <Biquad.h>

class Eq2Bands {
private:
    float samplerate = 44100;
    Biquad *bass;
    Biquad *treble;

    void applyBiquadFilter(float *out, const float *in, Biquad *filter, uint32_t n_samples) {
        for(uint32_t i=0; i<n_samples; i++) {
            out[i] = filter->process(in[i]);
        }
    }

public:
    float bass_freq;
    float treble_freq;

    Eq2Bands(float sr) : samplerate(sr) {
        // Setup equalizer section
        bass_freq = 250.0f;
        bass = new Biquad(bq_type_highpass, bass_freq / samplerate, 0.707f, 0.0f);
        treble_freq = 1500.0f;
        treble = new Biquad(bq_type_lowpass, treble_freq / samplerate, 0.707f, 0.0f);
    }

    void updateBass() {
        bass->setBiquad(bq_type_highpass, bass_freq / samplerate, 0.707f, 0.0f);
    }

    void updateTreble() {
        treble->setBiquad(bq_type_lowpass, treble_freq / samplerate, 0.707f, 0.0f);
    }

    void process(int count, const float* input, float* output) {
        applyBiquadFilter(output, input, bass, count);
        applyBiquadFilter(output, output, treble, count);
    }
};

#endif // EQ2BANDS_HPP