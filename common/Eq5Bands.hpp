/*
 * AIDA-X Eq5Bands base class for Chaos Audio
 * Copyright (C) 2024 Massimo Pennazio <maxipenna@libero.it>
 * SPDX-License-Identifier: GPL-3.0-or-later
*/

#ifndef EQ5BANDS_HPP
#define EQ5BANDS_HPP

#include <cstdint>

#include <Biquad.h>

/* Defines for tone controls */
#define PEAK 0.0f
#define BANDPASS 1.0f
#define DEPTH_FREQ 75.0f
#define DEPTH_Q 0.707f
#define PRESENCE_FREQ 900.0f
#define PRESENCE_Q 0.707f

class Eq5Bands {
private:
    float samplerate = 44100;
    Biquad *bass;
    Biquad *mid;
    Biquad *treble;
    Biquad *depth;
    Biquad *presence;

    void applyBiquadFilter(float *out, const float *in, Biquad *filter, uint32_t n_samples) {
        for(uint32_t i=0; i<n_samples; i++) {
            out[i] = filter->process(in[i]);
        }
    }

public:
    float bass_boost_db;
    float bass_freq;
    float mid_boost_db;
    float mid_freq;
    float mid_q;
    uint8_t mid_type;
    float treble_boost_db;
    float treble_freq;
    float depth_boost_db;
    float presence_boost_db;

    Eq5Bands(float sr) : samplerate(sr) {
        // Setup equalizer section
        bass_boost_db = 0.0f;
        bass_freq = 250.0f;
        bass = new Biquad(bq_type_lowshelf, bass_freq / samplerate, 0.707f, bass_boost_db);
        mid_boost_db = 0.0f;
        mid_freq = 600.0f;
        mid_q = 0.707f;
        mid_type = 0;
        mid = new Biquad(bq_type_peak, mid_freq / samplerate, mid_q, mid_boost_db);
        treble_boost_db = 0.0f;
        treble_freq = 1500.0f;
        treble = new Biquad(bq_type_highshelf, treble_freq / samplerate, 0.707f, treble_boost_db);
        depth_boost_db = 0.0f;
        depth = new Biquad(bq_type_peak, DEPTH_FREQ / samplerate, DEPTH_Q, depth_boost_db);
        presence_boost_db = 0.0f;
        presence = new Biquad(bq_type_highshelf, PRESENCE_FREQ / samplerate, PRESENCE_Q, presence_boost_db);
    }

    void updateBass() {
        bass->setBiquad(bq_type_lowshelf, bass_freq / samplerate, 0.707f, bass_boost_db);
    }

    void updateMid() {
        if(mid_type) {
            mid->setBiquad(bq_type_bandpass, mid_freq / samplerate, mid_q, mid_boost_db);
        }
        else {
           mid->setBiquad(bq_type_peak, mid_freq / samplerate, mid_q, mid_boost_db);
        }
    }

    void updateTreble() {
        treble->setBiquad(bq_type_highshelf, treble_freq / samplerate, 0.707f, treble_boost_db);
    }

    void updateDepth() {
        depth->setBiquad(bq_type_peak, DEPTH_FREQ / samplerate, DEPTH_Q, depth_boost_db);
    }

    void updatePresence() {
        presence->setBiquad(bq_type_highshelf, PRESENCE_FREQ / samplerate, PRESENCE_Q, presence_boost_db);
    }

    void process(int count, const float* input, float* output) {
        applyBiquadFilter(output, input, bass, count);
        applyBiquadFilter(output, output, mid, count);
        applyBiquadFilter(output, output, treble, count);
        applyBiquadFilter(output, output, depth, count);
        applyBiquadFilter(output, output, presence, count);
    }
};

#endif // EQ5BANDS_HPP