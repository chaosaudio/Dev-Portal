 /* ------------------------------------------------------------
name: "pluto"
Code generated with Faust 2.37.3 (https://faust.grame.fr)
Compilation options: -lang cpp -es 1 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __mydsp_H__
#define  __mydsp_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include "dsp.hpp"

class mydspSIG0 {
	
  private:
	
	int iVec1[2];
	int iRec4[2];
	
  public:
	
	int getNumInputsmydspSIG0() {
		return 0;
	}
	int getNumOutputsmydspSIG0() {
		return 1;
	}
	
	void instanceInitmydspSIG0(int sample_rate) {
		for (int l4 = 0; (l4 < 2); l4 = (l4 + 1)) {
			iVec1[l4] = 0;
		}
		for (int l5 = 0; (l5 < 2); l5 = (l5 + 1)) {
			iRec4[l5] = 0;
		}
	}
	
	void fillmydspSIG0(int count, float* table) {
		for (int i1 = 0; (i1 < count); i1 = (i1 + 1)) {
			iVec1[0] = 1;
			iRec4[0] = ((iVec1[1] + iRec4[1]) % 65536);
			table[i1] = std::sin((9.58738019e-05f * float(iRec4[0])));
			iVec1[1] = iVec1[0];
			iRec4[1] = iRec4[0];
		}
	}

};

static mydspSIG0* newmydspSIG0() { return (mydspSIG0*)new mydspSIG0(); }
static void deletemydspSIG0(mydspSIG0* dsp) { delete dsp; }

static float ftbl0mydspSIG0[65536];

#ifndef FAUSTCLASS 
#define FAUSTCLASS mydsp
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

class pluto : public dsp {

	
 private:
	
	FAUSTFLOAT fVslider0;
	float fRec0[2];
	int IOTA;
	float fVec0[131072];
	FAUSTFLOAT fVslider1;
	float fRec2[2];
	FAUSTFLOAT fVslider2;
	float fRec3[2];
	int fSampleRate;
	float fConst0;
	float fRec5[2];
	float fRec1[2];
	
 public:
	
	
    void instanceConstants() {
		knobs[0] = 5.0;
		version = "0.1.1";
		knobs[1] = 5.0;
		instanceClear();
		fSampleRate = 44100;
        mydspSIG0* sig0 = newmydspSIG0();
        sig0->instanceInitmydspSIG0(fSampleRate);
        sig0->fillmydspSIG0(65536, ftbl0mydspSIG0);
        deletemydspSIG0(sig0);
		knobs[2] = 5.0;
		fConst0 = (0.0166666675f / std::min<float>(192000.0f, std::max<float>(1.0f, float(fSampleRate))));
	}
	
	void instanceClear() {
		for (int l0 = 0; (l0 < 2); l0 = (l0 + 1)) {
			fRec0[l0] = 0.0f;
		}
		IOTA = 0;
		for (int l1 = 0; (l1 < 131072); l1 = (l1 + 1)) {
			fVec0[l1] = 0.0f;
		}
		for (int l2 = 0; (l2 < 2); l2 = (l2 + 1)) {
			fRec2[l2] = 0.0f;
		}
		for (int l3 = 0; (l3 < 2); l3 = (l3 + 1)) {
			fRec3[l3] = 0.0f;
		}
		for (int l6 = 0; (l6 < 2); l6 = (l6 + 1)) {
			fRec5[l6] = 0.0f;
		}
		for (int l7 = 0; (l7 < 2); l7 = (l7 + 1)) {
			fRec1[l7] = 0.0f;
		}
	}

	void compute(int count, FAUSTFLOAT* input0, FAUSTFLOAT* output0) {
		float fSlow0 = (0.00700000022f * float(knobs[1]));
		float fSlow1 = (0.00700000022f * float(knobs[0]));
		float fSlow2 = (0.00700000022f * float(knobs[2]));
		for (int i0 = 0; (i0 < count); i0 = (i0 + 1)) {
			fRec0[0] = (fSlow0 + (0.992999971f * fRec0[1]));
			float fTemp0 = float(input0[i0]);
			fVec0[(IOTA & 131071)] = fTemp0;
			fRec2[0] = (fSlow1 + (0.992999971f * fRec2[1]));
			fRec3[0] = (fSlow2 + (0.992999971f * fRec3[1]));
			float fTemp1 = (fRec5[1] + (fConst0 * fRec2[0]));
			fRec5[0] = (fTemp1 - std::floor(fTemp1));
			fRec1[0] = std::fmod((fRec1[1] + (513.0f - std::pow(2.0f, (0.0833333358f * ((std::min<float>((0.00416666688f * fRec2[0]), 1.0f) * (0.0f - (0.100000001f * (1.0f - std::pow(10.0f, (0.100000001f * fRec3[0])))))) * ftbl0mydspSIG0[int((65536.0f * fRec5[0]))]))))), 512.0f);
			int iTemp2 = int(fRec1[0]);
			float fTemp3 = std::floor(fRec1[0]);
			float fTemp4 = std::min<float>((0.00390625f * fRec1[0]), 1.0f);
			float fTemp5 = (fRec1[0] + 512.0f);
			int iTemp6 = int(fTemp5);
			float fTemp7 = std::floor(fTemp5);
			output0[i0] = FAUSTFLOAT((0.200000003f * (fRec0[0] * ((((fVec0[((IOTA - std::min<int>(65537, std::max<int>(0, iTemp2))) & 131071)] * (fTemp3 + (1.0f - fRec1[0]))) + ((fRec1[0] - fTemp3) * fVec0[((IOTA - std::min<int>(65537, std::max<int>(0, (iTemp2 + 1)))) & 131071)])) * fTemp4) + (((fVec0[((IOTA - std::min<int>(65537, std::max<int>(0, iTemp6))) & 131071)] * (fTemp7 + (-511.0f - fRec1[0]))) + (fVec0[((IOTA - std::min<int>(65537, std::max<int>(0, (iTemp6 + 1)))) & 131071)] * (fRec1[0] + (512.0f - fTemp7)))) * (1.0f - fTemp4))))));
			fRec0[1] = fRec0[0];
			IOTA = (IOTA + 1);
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec5[1] = fRec5[0];
			fRec1[1] = fRec1[0];
		}
	}

};
	extern "C" {
	dsp * create() {
		return new pluto;
	}
}

#endif
