/*
 * AIDA-X Equalizer plugin for Chaos Audio
 * Copyright (C) 2024 Massimo Pennazio <maxipenna@libero.it>
 * SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dsp.hpp"

#include <Eq5Bands.hpp>

class Equalizer : public dsp {
private:
    Eq5Bands* eq5Bands;

public:
    float knobs_old[MAXKNOBS];
    SWITCH_STATE switches_old[MAXSWITCHES];

    Equalizer() {
        // Initialize your plugin here
        eq5Bands = nullptr;
    }

    ~Equalizer() {
        // Clean up your plugin here
        if (eq5Bands != nullptr)
            delete eq5Bands;
    }

    virtual void instanceConstants() override {
        // Implement the pure virtual function from the base class here
        // Set the constants for your plugin here
        version = "0.9.0";
        name = "Equalizer";
        if (eq5Bands != nullptr) {
            delete eq5Bands;
        }
        eq5Bands = new Eq5Bands(getSampleRate());
        // Initialize controls, mapping to the correct values
        knobs[0] = MAP(eq5Bands->bass_boost_db, -8.0f, 8.0f, 0.0f, 10.0f);
        knobs[1] = MAP(eq5Bands->mid_boost_db, -8.0f, 8.0f, 0.0f, 10.0f);
        knobs[2] = MAP(eq5Bands->treble_boost_db, -8.0f, 8.0f, 0.0f, 10.0f);
        knobs[3] = MAP(eq5Bands->depth_boost_db, -8.0f, 8.0f, 0.0f, 10.0f);
        knobs[4] = MAP(eq5Bands->presence_boost_db, -8.0f, 8.0f, 0.0f, 10.0f);
        knobs[5] = MAP(eq5Bands->mid_freq, 150.0f, 5000.0f, 0.0f, 10.0f);
        switches[1] = eq5Bands->mid_type == 0 ? DOWN : UP;
        knobs_old[0] = knobs[0];
        knobs_old[1] = knobs[1];
        knobs_old[2] = knobs[2];
        knobs_old[3] = knobs[3];
        knobs_old[4] = knobs[4];
        knobs_old[5] = knobs[5];
        switches_old[1] = switches[1];
    }

    virtual void compute(int count, FAUSTFLOAT* input0, FAUSTFLOAT* output0) override {
        // Implement the compute method here
        uint8_t bass_has_changed = 0;
        uint8_t mid_has_changed = 0;
        uint8_t treble_has_changed = 0;
        uint8_t depth_has_changed = 0;
        uint8_t presence_has_changed = 0;

        if (eq5Bands == nullptr) {
            return;
        } else {
            /* Bass */
            if (knobs[0] != knobs_old[0]) {
                knobs_old[0] = knobs[0];
                eq5Bands->bass_boost_db = MAP(knobs_old[0], 0.0f, 10.0f, -8.0f, 8.0f);
                bass_has_changed++;
            }
            if (bass_has_changed) {
                eq5Bands->updateBass();
            }
            /* Mid */
            if (knobs[1] != knobs_old[1]) {
                knobs_old[1] = knobs[1];
                eq5Bands->mid_boost_db = MAP(knobs_old[1], 0.0f, 10.0f, -8.0f, 8.0f);
                mid_has_changed++;
            }
            if (knobs[5] != knobs_old[5]) {
                knobs_old[5] = knobs[5];
                eq5Bands->mid_freq = MAP(knobs_old[5], 0.0f, 10.0f, 150.0f, 5000.0f);
                mid_has_changed++;
            }
            if (switches[1] != switches_old[1]) {
                switches_old[1] = switches[1];
                eq5Bands->mid_type = switches_old[1] == DOWN ? 0 : 1;
                mid_has_changed++;
            }
            if (mid_has_changed) {
                eq5Bands->updateMid();
            }
            /* Treble */
            if (knobs[2] != knobs_old[2]) {
                knobs_old[2] = knobs[2];
                eq5Bands->treble_boost_db = MAP(knobs_old[2], 0.0f, 10.0f, -8.0f, 8.0f);
                treble_has_changed++;
            }
            if (treble_has_changed) {
                eq5Bands->updateTreble();
            }
            /* Depth */
            if (knobs[3] != knobs_old[3]) {
                knobs_old[3] = knobs[3];
                eq5Bands->depth_boost_db = MAP(knobs_old[3], 0.0f, 10.0f, -8.0f, 8.0f);
                depth_has_changed++;
            }
            if (depth_has_changed) {
                eq5Bands->updateDepth();
            }
            /* Presence */
            if (knobs[4] != knobs_old[4]) {
                knobs_old[4] = knobs[4];
                eq5Bands->presence_boost_db = MAP(knobs_old[4], 0.0f, 10.0f, -8.0f, 8.0f);
                presence_has_changed++;
            }
            if (presence_has_changed) {
                eq5Bands->updatePresence();
            }
            /* Apply filters */
            eq5Bands->process(count, input0, output0);
        }
    }

    // If you need to override any other methods from the base class, you can do so here
};

extern "C" dsp* create() {
    return new Equalizer();
}

extern "C" void destroy(dsp* p) {
    delete p;
}

extern "C" const char* dsp_version = DSP_VERSION;
