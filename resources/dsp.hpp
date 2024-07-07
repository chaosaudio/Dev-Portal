#ifndef __dsp__
#define __dsp__

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <math.h>

#include <algorithm>
#include <random>
#include <chrono>

#define DSP_VERSION "2.0.0"

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

#define MAXKNOBS 10
#define MAXSWITCHES 5
#define MAXFILES 5

#ifndef Uint
typedef unsigned int Uint;
#endif

/* Convert a value in dB's to a coefficent */
#define DB_CO(g) ((g) > -90.0f ? powf(10.0f, (g) * 0.05f) : 0.0f)
#define CO_DB(v) (20.0f * log10f(v))

/* Define a macro to scale % to coeff */
#define PC_CO(g) ((g) < 100.0f ? (g / 100.0f) : 1.0f)

/* Define a macro to re-maps a number from one range to another  */
#define MAP(x, in_min, in_max, out_min, out_max) (((x - in_min) * (out_max - out_min) / (in_max - in_min)) + out_min)

/* Define a macro to implement a selector out of a knob */
#define SELECTOR(knob, n) ((uint8_t)(knob * n) % n)

struct dsp {
	enum SWITCH_STATE{
		UP = 0,
		DOWN = 1,
		MIDDLE = 2
	};
    private:
		int fSampleRate = 44100;
		std::string filePaths[MAXFILES];
		bool trailsSetting = false;

	protected:
		std::string version;
    
    public:
		float knobs[MAXKNOBS];
		SWITCH_STATE switches[MAXSWITCHES];
		SWITCH_STATE stompSwitch;
		std::string name;
		bool trailsAvailable = false;

        dsp() {
			name = "null";
			version = "0.0.0";
			for(int i=0;i<MAXFILES;++i)
				filePaths[i]="";
        	for(int i=0;i<MAXKNOBS;++i)
        		knobs[i]=.5;
        	for(int i=0;i<MAXSWITCHES;++i)
        		switches[i]=SWITCH_STATE::DOWN;
        	stompSwitch = DOWN;
        }
        ~dsp() {}
	
	//Use for switch debugging
	//static const int SWITCH_STATES = 3;
    //const char* swStates[SWITCH_STATES] = {"UP", "MIDDLE", "DOWN"};

	void getTextForEnum(SWITCH_STATE enumVal, std::string *out){
		if(enumVal == 0)//SWITCH_STATE::UP)
			*out = "UP";
		else if(enumVal == 1)//SWITCH_STATE::MIDDLE)
			*out = "MIDDLE";
		else if(enumVal == 2)//SWITCH_STATE::DOWN)
			*out = "DOWN";
		else
			*out = "BAD";
		return;
	}

	void setName(std::string name){
		this->name=name;
	}
	
	std::string getName(){
		return name;
	}
	
	std::string getVersion(){
		return version;
	}

	void setFilePath(std::string path, uint8_t index){
		if(index<MAXFILES)
			this->filePaths[index]=path;
	}

	void setFilePath(std::string path){
		this->filePaths[0]=path;
	}

	std::string getFilePath(uint8_t index){
		if(index<MAXFILES)
			return filePaths[index];
		else
			return "";
	}

	std::string getFilePath(){
		return filePaths[0];
	}

	std::string extractFileName(const std::string& path){
		size_t pos = path.find_last_of("/\\");
		if(pos != std::string::npos)
			return path.substr(pos + 1);
		else
			return "";
	}

	std::string getFileName(uint8_t index){
		if(index<MAXFILES){
			return extractFileName(filePaths[index]);
		}
		else
			return "";
	}

	std::string getFileName(){
		return extractFileName(filePaths[0]);
	}

	void setSampleRate(int sampleRate){
		fSampleRate = sampleRate;
	}

	int getSampleRate(){
		return fSampleRate;
	}

	int getNyquist(){
		return fSampleRate/2;
	}

	bool getTrailsVal(){
		return trailsSetting;
	}

	void setTrailsVal(bool val){
		if (trailsAvailable) {
			trailsSetting = val;
		}
	}

	virtual void setKnob(int num, float knobVal){
		this->knobs[num] = knobVal;
	}
	
	virtual float getKnob(int in){
		return knobs[in];
	}
	
	virtual void setSwitch(int num, SWITCH_STATE switchVal){
		switches[num] = switchVal;
	}
	
	virtual SWITCH_STATE getSwitch(int in){
		return switches[in];
	}

	virtual void setStompSwitch(SWITCH_STATE switchVal){
		stompSwitch=switchVal;
	}
	
	virtual bool getStompSwitch(){
		return stompSwitch;
	}
	
	virtual void stompSwitchPressed(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs){
		if(stompSwitch){
			compute(count, inputs, outputs);
		}
		return;
	}
	
	virtual void instanceConstants() = 0;

	virtual void compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) = 0;

	virtual void benchmark(int seconds) {
		printf("Starting benchmark\n");
		const auto n_samples = static_cast<size_t>(fSampleRate * seconds);

		// Generate input signal
		printf("Generating input signal\n");
		std::vector<float> input(n_samples);
		std::default_random_engine generator;
		std::uniform_real_distribution<float> distribution(-1.0, 1.0);
		std::generate(input.begin(), input.end(), [&]() { return distribution(generator); });

		std::vector<float> output(n_samples);

		// Run benchmark
		using clock_t = std::chrono::high_resolution_clock;
		using second_t = std::chrono::duration<float>;
		printf("Computing %d seconds of data @%dHz\n", seconds, fSampleRate);
		auto start = clock_t::now();
		compute(n_samples, input.data(), output.data());
		float dur = std::chrono::duration_cast<second_t>(clock_t::now() - start).count();
		printf("Processed %d seconds of signal in %f seconds\n", seconds, dur);
		printf("%f x real-time\n", seconds / dur);
	}
};

#endif // __dsp__

using dsp_creator_t = dsp *(*)();
