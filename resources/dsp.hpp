#ifndef __dsp__
#define __dsp__

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <math.h>

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

#define MAXKNOBS 10
#define MAXSWITCHES 5

#ifndef Uint
typedef unsigned int Uint;
#endif

struct dsp {
	enum SWITCH_STATE{
		UP = 0,
		DOWN = 1,
		MIDDLE = 2
	};
    private:

	protected:
		std::string version;
    
    public:
		int fSampleRate = 44100;
		float knobs[MAXKNOBS];
		SWITCH_STATE switches[MAXSWITCHES];
		SWITCH_STATE stompSwitch;
		std::string name;
		
        dsp() {
        	name = "null";
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
};

#endif



using dsp_creator_t = dsp *(*)();
