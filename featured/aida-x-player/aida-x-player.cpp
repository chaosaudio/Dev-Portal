/*
 * AIDA-X Player plugin for Chaos Audio
 * Copyright (C) 2024 Massimo Pennazio <maxipenna@libero.it>
 * SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dsp.hpp"

#include <ValueSmoother.hpp>
#include <RTNeural/RTNeural.h>
#include <model_variant.hpp>

// @TODO: unsupported for now
#define AIDADSP_CONDITIONED_MODELS 0

struct DynamicModel {
    ModelVariantType variant;
    char* path;
    bool input_skip; /* Means the model has been trained with first input element skipped to the output */
    float input_gain;
    float output_gain;
    float samplerate;
#if AIDADSP_CONDITIONED_MODELS
    LinearValueSmoother param1Coeff;
    LinearValueSmoother param2Coeff;
    bool paramFirstRun;
#endif
};

class AidaXPlayer : public dsp {
private:
    // Define your private variables here
    DynamicModel* models[MAXFILES] = {nullptr};
    uint8_t modelIndex;

    /**
     * This function carries model calculations for snapshot models, models with one parameter and
     * models with two parameters.
    */
    void applyModel(DynamicModel* model, float* out, uint32_t n_samples)
    {
        const bool input_skip = model->input_skip;
        const float input_gain = model->input_gain;
        const float output_gain = model->output_gain;
    #if AIDADSP_CONDITIONED_MODELS
        LinearValueSmoother& param1Coeff = model->param1Coeff;
        LinearValueSmoother& param2Coeff = model->param2Coeff;
    #endif

        std::visit (
            [input_skip, &out, n_samples, input_gain, output_gain
    #if AIDADSP_CONDITIONED_MODELS
            , &param1Coeff, &param2Coeff
    #endif
            ] (auto&& custom_model)
            {
                using ModelType = std::decay_t<decltype (custom_model)>;
                if constexpr (ModelType::input_size == 1)
                {
                    if (input_skip)
                    {
                        for (uint32_t i=0; i<n_samples; ++i) {
                            out[i] *= input_gain;
                            out[i] += custom_model.forward (out + i);
                            out[i] *= output_gain;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<n_samples; ++i) {
                            out[i] *= input_gain;
                            out[i] = custom_model.forward (out + i);
                            out[i] *= output_gain;
                        }
                    }
                }
    #if AIDADSP_CONDITIONED_MODELS
                else if constexpr (ModelType::input_size == 2)
                {
                    float inArray1 alignas(RTNEURAL_DEFAULT_ALIGNMENT)[2] = { 0.0, 0.0 };
                    if (input_skip)
                    {
                        for (uint32_t i=0; i<n_samples; ++i) {
                            out[i] *= input_gain;
                            inArray1[0] = out[i];
                            inArray1[1] = param1Coeff.next();
                            out[i] += custom_model.forward (inArray1);
                            out[i] *= output_gain;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<n_samples; ++i) {
                            out[i] *= input_gain;
                            inArray1[0] = out[i];
                            inArray1[1] = param1Coeff.next();
                            out[i] = custom_model.forward (inArray1);
                            out[i] *= output_gain;
                        }
                    }
                }
                else if constexpr (ModelType::input_size == 3)
                {
                    float inArray2 alignas(RTNEURAL_DEFAULT_ALIGNMENT)[3] = { 0.0, 0.0, 0.0 };
                    if (input_skip)
                    {
                        for (uint32_t i=0; i<n_samples; ++i) {
                            out[i] *= input_gain;
                            inArray2[0] = out[i];
                            inArray2[1] = param1Coeff.next();
                            inArray2[2] = param2Coeff.next();
                            out[i] += custom_model.forward (inArray2);
                            out[i] *= output_gain;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<n_samples; ++i) {
                            out[i] *= input_gain;
                            inArray2[0] = out[i];
                            inArray2[1] = param1Coeff.next();
                            inArray2[2] = param2Coeff.next();
                            out[i] = custom_model.forward (inArray2);
                            out[i] *= output_gain;
                        }
                    }
                }
    #endif
            },
            model->variant
        );
    }

    /**
     * This function loads a pre-trained neural model from a json file
    */
    DynamicModel* loadModelFromPath(const char* path) {
        // Load the model from the path
        int input_skip;
        int input_size;
        float input_gain;
        float output_gain;
        float model_samplerate = 0.0f;
        nlohmann::json model_json;

        try {
            std::ifstream jsonStream(path, std::ifstream::binary);
            jsonStream >> model_json;

            /* Understand which model type to load */
            input_size = model_json["in_shape"].back().get<int>();
            if (input_size > MAX_INPUT_SIZE) {
                throw std::invalid_argument("Value for input_size not supported");
            }

            if (model_json["in_skip"].is_number()) {
                input_skip = model_json["in_skip"].get<int>();
                if (input_skip > 1)
                    throw std::invalid_argument("Values for in_skip > 1 are not supported");
            }
            else {
                input_skip = 0;
            }

            if (model_json["in_gain"].is_number()) {
                input_gain = DB_CO(model_json["in_gain"].get<float>());
            }
            else {
                input_gain = 1.0f;
            }

            if (model_json["out_gain"].is_number()) {
                output_gain = DB_CO(model_json["out_gain"].get<float>());
            }
            else {
                output_gain = 1.0f;
            }

            if (model_json["metadata"]["samplerate"].is_number()) {
                model_samplerate = model_json["metadata"]["samplerate"].get<float>();
            }
            else if (model_json["samplerate"].is_number()) {
                model_samplerate = model_json["samplerate"].get<float>();
            }
            else {
                throw std::invalid_argument("No samplerate was present in the model file");
            }

            if (model_samplerate != float(getSampleRate())) {
                throw std::invalid_argument("Model samplerate does not match with plugin samplerate");
            }

            printf("Successfully loaded json file: %s\n", path);
        }
        catch (const std::exception& e) {
            printf("Unable to load json file: %s\nError: %s\n", path, e.what());
            return nullptr;
        }

        std::unique_ptr<DynamicModel> model = std::make_unique<DynamicModel>();

        try {
            if (! custom_model_creator (model_json, model->variant))
                throw std::runtime_error ("Unable to identify a known model architecture!");

            std::visit (
                [&model_json] (auto&& custom_model)
                {
                    using ModelType = std::decay_t<decltype (custom_model)>;
                    if constexpr (! std::is_same_v<ModelType, NullModel>)
                    {
                        custom_model.parseJson (model_json, true);
                        custom_model.reset();
                    }
                },
                model->variant);
            printf("%s %d: mdl rst!\n", __func__, __LINE__);
        }
        catch (const std::exception& e) {
            printf("Error loading model: %s\n", e.what());
            return nullptr;
        }

        /* Save extra info */
        model->path = strdup(path);
        model->input_skip = input_skip != 0;
        model->input_gain = input_gain;
        model->output_gain = output_gain;
        model->samplerate = model_samplerate;
    #if AIDADSP_CONDITIONED_MODELS
        model->param1Coeff.setSampleRate(model_samplerate);
        model->param1Coeff.setTimeConstant(0.1f);
        model->param1Coeff.setTargetValue(old_param1);
        model->param1Coeff.clearToTargetValue();
        model->param2Coeff.setSampleRate(model_samplerate);
        model->param2Coeff.setTimeConstant(0.1f);
        model->param2Coeff.setTargetValue(old_param2);
        model->param2Coeff.clearToTargetValue();
        model->paramFirstRun = true;
    #endif

        /* @TODO: init */
        float out[2048] = {};
        applyModel(model.get(), out, 2048);

        return model.release();
    }

public:
    float knobs_old[MAXKNOBS];

    AidaXPlayer() {
        // Initialize your plugin here
    }

    ~AidaXPlayer() {
        // Clean up your plugin here
        for (int i=0; i<MAXFILES; ++i) {
            if (models[i] != nullptr) {
                delete models[i];
                models[i] = nullptr;
            }
        }
    }

    /* @TODO: warning!!! This is heavy and should NEVER be used from an audio rt thread */
    void loadModels() {
        for (int i=0; i<MAXFILES; ++i) {
            if (models[i] != nullptr) {
                delete models[i];
                models[i] = nullptr;
            }
            if (getFilePath(i).empty()) {
                continue;
            }
            printf("Loading model from path: %s\n", getFilePath(i).c_str());
            DynamicModel* newmodel = loadModelFromPath(getFilePath(i).c_str());
            if (newmodel == nullptr) {
                printf("Error loading model from path: %s\n", getFilePath(i).c_str());
                continue;
            }
            else {
                models[i] = newmodel;
            }
        }
    }

    virtual void instanceConstants() override {
        // Implement the pure virtual function from the base class here
        // Set the constants for your plugin here
        version = "0.9.0";
        name = "aida-x-player";
        // Initialize controls, mapping to the correct values
        knobs[0] = 0.0f;
        knobs_old[0] = knobs[0];
        modelIndex = SELECTOR(knobs[0], MAXFILES);
        loadModels();
    }

    virtual void compute(int count, FAUSTFLOAT* input0, FAUSTFLOAT* output0) override {
        // Implement the compute method here
        // Update model index only if the knob has changed
        if (knobs[0] != knobs_old[0]) {
            knobs_old[0] = knobs[0];
            modelIndex = SELECTOR(knobs[0], MAXFILES);
        }
        // Apply model if valid
        if (models[modelIndex] == nullptr) {
            return;
        } else {
            applyModel(models[modelIndex], output0, count);
        }
    }

    // If you need to override any other methods from the base class, you can do so here
};

extern "C" dsp* create() {
    return new AidaXPlayer();
}

extern "C" void destroy(dsp* p) {
    delete p;
}

extern "C" const char* dsp_version = DSP_VERSION;
