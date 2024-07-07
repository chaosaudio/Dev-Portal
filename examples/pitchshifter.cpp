#ifndef  __mydsp_H__
#define  __mydsp_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include "dsp.hpp"
#include "signalsmith-stretch.h"

#ifndef FAUSTCLASS 
#define FAUSTCLASS mydsp
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

class pitchshifter : public dsp {

 private:
	
    int sampleRate = 44100;
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    int channels = 1;
    float* inputBuffers[1];
    float* outputBuffers[1];
	
 public:
		
 void instanceConstants() {
        stretch.presetDefault(channels, sampleRate);
        stretch.setTransposeSemitones(12, 8000/sampleRate);
		version = "0.1.1";
	}

	void instanceClear() {

	}
	
	void compute(int count, FAUSTFLOAT* input0, FAUSTFLOAT* output0) {
        
        inputBuffers[0] = input0;
        outputBuffers[0] = output0;
        
		for (int i0 = 0; (i0 < count); i0 = (i0 + 1)) {
            stretch.process(inputBuffers, count, outputBuffers, count);
		}
        stretch.reset();
        
	}

};
	extern "C" {
	dsp * create() {
		return new pitchshifter;
	}
}

#endif
