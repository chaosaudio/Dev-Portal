/*
 * AIDA-X Convolver plugin for Chaos Audio
 * Copyright (C) 2024 Massimo Pennazio <maxipenna@libero.it>
 * SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dsp.hpp"

#include <Eq2Bands.hpp>
#include <ValueSmoother.hpp>

/* 3rd-party  */
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

// -Wunused-variable
#include "CDSPResampler.h"

// Must be last
#include <FFTConvolver.h>

using namespace fftconvolver;

class AidaXConvolver : public dsp {
private:
    // Define your private variables here
    FFTConvolver* irs[MAXFILES] = {nullptr};
    uint16_t irBlockSize = 1024;
    uint8_t irIndex;
    Eq2Bands* eq2Bands = nullptr;

public:
    float knobs_old[MAXKNOBS];

    AidaXConvolver() {
        // Initialize your plugin here
    }

    ~AidaXConvolver() {
        // Clean up your plugin here
        for (int i=0; i<MAXFILES; ++i) {
            if (irs[i] != nullptr) {
                delete irs[i];
                irs[i] = nullptr;
            }
        }
        if (eq2Bands != nullptr) {
            delete eq2Bands;
        }
    }

    uint16_t getIrBlockSize() {
        return irBlockSize;
    }

    /* @TODO: warning!!! This is heavy and should NEVER be used from an audio rt thread */
    void loadImpulseResponses() {
        printf("Loading impulse responses\n");
        printf("Ir block size: %d\n", getIrBlockSize());
        for (int i=0; i<MAXFILES; ++i) {
            if (irs[i] != nullptr) {
                delete irs[i];
                irs[i] = nullptr;
            }
            if (getFilePath(i).empty()) {
                continue;
            }
            printf("Loading ir from path: %s\n", getFilePath(i).c_str());
            // Understand extension of the file, if .wav or .flac
            std::string ext = getFilePath(i).substr(getFilePath(i).find_last_of(".") + 1);
            if (ext == "wav") {
                drwav_uint64 fileTotalFrames;
                unsigned int fileSampleRate;
                unsigned int fileChannels;
                float* data = drwav_open_file_and_read_pcm_frames_f32(getFilePath(i).c_str(), &fileChannels, &fileSampleRate, &fileTotalFrames, NULL);
                if (data == nullptr) {
                    printf("Error loading ir from path: %s\n", getFilePath(i).c_str());
                    continue;
                }
                if (fileSampleRate != getSampleRate()) {
                    r8b::CDSPResampler16IR resampler(fileSampleRate, getSampleRate(), fileTotalFrames);
                    const int numResampledFrames = resampler.getMaxOutLen(0);
                    if (numResampledFrames > 0) {
                        float* resampledData = new float[numResampledFrames];
                        printf("Resampling ir from %d to %d\n", fileSampleRate, getSampleRate());
                        resampler.oneshot(data, fileTotalFrames, resampledData, numResampledFrames);
                        data = resampledData;
                        fileTotalFrames = numResampledFrames;
                    } else {
                        drwav_free(data, NULL);
                        continue;
                    }
                }
                if (fileChannels > 1) {
                    // Assuming multi-channel in an interleaved format, convert to mono
                    printf("Converting multi-channel file %s to mono\n", getFilePath(i).c_str());
                    for (drwav_uint64 i=0, j=0; j<fileTotalFrames; ++i, j+=fileChannels)
                        data[i] = data[j];
                    fileTotalFrames /= fileChannels;
                }
                // @TODO: perform ir volume normalization here
                irs[i] = new FFTConvolver();
                if (!irs[i]->init(getIrBlockSize(), data, fileTotalFrames)) {
                    printf("Error loading ir from path: %s\n", getFilePath(i).c_str());
                    delete irs[i];
                    irs[i] = nullptr;
                }
                drwav_free(data, NULL);
            } else {
                // @TODO: implement loading of flac files
                printf("Unsupported format: %s\n", getFilePath(i).c_str());
            }
        }
    }

    virtual void instanceConstants() override {
        // Implement the pure virtual function from the base class here
        // Set the constants for your plugin here
        version = "0.9.0";
        name = "aida-x-convolver";
        if (eq2Bands != nullptr) {
            delete eq2Bands;
        }
        eq2Bands = new Eq2Bands(getSampleRate());
        // Initialize controls, mapping to the correct values
        knobs[0] = 0.0f;
        knobs[1] = MAP(eq2Bands->bass_freq, 80.0f, 480.0f, 0.0f, 10.0f);
        knobs[2] = MAP(eq2Bands->treble_freq, 1500.0f, getNyquist(), 0.0f, 10.0f);
        switches[1] = UP; /* Bypass 2-bands equalizer */
        knobs_old[0] = knobs[0];
        knobs_old[1] = knobs[1];
        knobs_old[2] = knobs[2];
        irIndex = SELECTOR(knobs[0], MAXFILES);
        loadImpulseResponses();
    }

    virtual void compute(int count, FAUSTFLOAT* input0, FAUSTFLOAT* output0) override {
        // Implement the compute method here
        // Update ir index only if the knob has changed
        if (knobs[0] != knobs_old[0]) {
            knobs_old[0] = knobs[0];
            irIndex = SELECTOR(knobs[0], MAXFILES);
        }
        // Update eq2Bands only if enabled and if the knob has changed
        if (eq2Bands != nullptr && switches[1] == DOWN) {
            if (knobs[1] != knobs_old[1]) {
                knobs_old[1] = knobs[1];
                eq2Bands->bass_freq = MAP(knobs_old[1], 0.0f, 10.0f, 80.0f, 480.0f);
                eq2Bands->updateBass();
            }
            if (knobs[2] != knobs_old[2]) {
                knobs_old[2] = knobs[2];
                eq2Bands->treble_freq = MAP(knobs_old[2], 0.0f, 10.0f, 1500.0f, getNyquist());
                eq2Bands->updateTreble();
            }
            eq2Bands->process(count, input0, output0);
        } else {
            std::memcpy(output0, input0, sizeof(FAUSTFLOAT)*count);
        }
        // Apply ir if valid
        if (irs[irIndex] == nullptr) {
            return;
        } else {
            irs[irIndex]->process(output0, output0, count);
        }
    }

    // If you need to override any other methods from the base class, you can do so here
};

extern "C" dsp* create() {
    return new AidaXConvolver();
}

extern "C" void destroy(dsp* p) {
    delete p;
}

extern "C" const char* dsp_version = DSP_VERSION;
