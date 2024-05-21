/* ------------------------------------------------------------
name: "spectrometer"
version: "0.1.0"
Code generated with Faust 2.37.3 (https://faust.grame.fr)
Compilation options: -lang cpp -es 1 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __mydsp_H__
#define  __mydsp_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include "dsp.hpp"

static float mydsp_faustpower2_f(float value) {
	return (value * value);
}

#ifndef FAUSTCLASS 
#define FAUSTCLASS mydsp
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

class spectrometer : public dsp {

 private:
	
	int fSampleRate;
	float fConst3;
	float fConst6;
	float fConst9;
	float fConst12;
	float fConst15;
	float fConst17;
	float fConst18;
	FAUSTFLOAT fVslider0;
	float fConst19;
	float fRec5[3];
	float fConst20;
	FAUSTFLOAT fVslider1;
	float fConst21;
	float fRec4[3];
	float fConst22;
	FAUSTFLOAT fVslider2;
	float fConst23;
	float fRec3[3];
	float fConst24;
	FAUSTFLOAT fVslider3;
	float fConst25;
	float fRec2[3];
	float fConst26;
	FAUSTFLOAT fVslider4;
	float fConst27;
	float fRec1[3];
	float fConst28;
	FAUSTFLOAT fVslider5;
	float fConst29;
	float fRec0[3];
	
 public:
		
 void instanceConstants() {
		knobs[0] = 5.0;
		version = "0.1.1";
		knobs[1] = 5.0;
		fSampleRate = 44100;
		knobs[2] = 5.0;
		float fConst0 = std::min<float>(192000.0f, std::max<float>(1.0f, float(fSampleRate)));
		instanceClear();
		knobs[3] = 5.0;
		float fConst1 = (10053.0967f / fConst0);
		knobs[4] = 5.0;
		float fConst2 = std::tan(fConst1);
		knobs[5] = 5.0;
		fConst3 = (2.0f * (1.0f - (1.0f / mydsp_faustpower2_f(fConst2))));
		float fConst4 = (5026.54834f / fConst0);
		float fConst5 = std::tan(fConst4);
		fConst6 = (2.0f * (1.0f - (1.0f / mydsp_faustpower2_f(fConst5))));
		float fConst7 = (2513.27417f / fConst0);
		float fConst8 = std::tan(fConst7);
		fConst9 = (2.0f * (1.0f - (1.0f / mydsp_faustpower2_f(fConst8))));
		float fConst10 = (1256.63708f / fConst0);
		float fConst11 = std::tan(fConst10);
		fConst12 = (2.0f * (1.0f - (1.0f / mydsp_faustpower2_f(fConst11))));
		float fConst13 = (628.318542f / fConst0);
		float fConst14 = std::tan(fConst13);
		fConst15 = (2.0f * (1.0f - (1.0f / mydsp_faustpower2_f(fConst14))));
		float fConst16 = std::tan((314.159271f / fConst0));
		fConst17 = (2.0f * (1.0f - (1.0f / mydsp_faustpower2_f(fConst16))));
		fConst18 = (1.0f / fConst16);
		fConst19 = (314.159271f / (fConst0 * std::sin(fConst13)));
		fConst20 = (1.0f / fConst14);
		fConst21 = (628.318542f / (fConst0 * std::sin(fConst10)));
		fConst22 = (1.0f / fConst11);
		fConst23 = (1256.63708f / (fConst0 * std::sin(fConst7)));
		fConst24 = (1.0f / fConst8);
		fConst25 = (2513.27417f / (fConst0 * std::sin(fConst4)));
		fConst26 = (1.0f / fConst5);
		fConst27 = (5026.54834f / (fConst0 * std::sin(fConst1)));
		fConst28 = (1.0f / fConst2);
		fConst29 = (10053.0967f / (fConst0 * std::sin((20106.1934f / fConst0))));
	}

	void instanceClear() {
		for (int l0 = 0; (l0 < 3); l0 = (l0 + 1)) {
			fRec5[l0] = 0.0f;
		}
		for (int l1 = 0; (l1 < 3); l1 = (l1 + 1)) {
			fRec4[l1] = 0.0f;
		}
		for (int l2 = 0; (l2 < 3); l2 = (l2 + 1)) {
			fRec3[l2] = 0.0f;
		}
		for (int l3 = 0; (l3 < 3); l3 = (l3 + 1)) {
			fRec2[l3] = 0.0f;
		}
		for (int l4 = 0; (l4 < 3); l4 = (l4 + 1)) {
			fRec1[l4] = 0.0f;
		}
		for (int l5 = 0; (l5 < 3); l5 = (l5 + 1)) {
			fRec0[l5] = 0.0f;
		}
	}
	
	void compute(int count, FAUSTFLOAT* input0, FAUSTFLOAT* output0) {

		float fSlow0 = float(knobs[0]);
		int iSlow1 = (fSlow0 > 0.0f);
		float fSlow2 = (fConst19 * std::pow(10.0f, (0.0500000007f * std::fabs(fSlow0))));
		float fSlow3 = (iSlow1 ? fConst19 : fSlow2);
		float fSlow4 = (1.0f - (fConst18 * (fSlow3 - fConst18)));
		float fSlow5 = ((fConst18 * (fConst18 + fSlow3)) + 1.0f);
		float fSlow6 = (iSlow1 ? fSlow2 : fConst19);
		float fSlow7 = ((fConst18 * (fConst18 + fSlow6)) + 1.0f);
		float fSlow8 = (1.0f - (fConst18 * (fSlow6 - fConst18)));
		float fSlow9 = float(knobs[1]);
		int iSlow10 = (fSlow9 > 0.0f);
		float fSlow11 = (fConst21 * std::pow(10.0f, (0.0500000007f * std::fabs(fSlow9))));
		float fSlow12 = (iSlow10 ? fConst21 : fSlow11);
		float fSlow13 = (1.0f - (fConst20 * (fSlow12 - fConst20)));
		float fSlow14 = ((fConst20 * (fConst20 + fSlow12)) + 1.0f);
		float fSlow15 = (iSlow10 ? fSlow11 : fConst21);
		float fSlow16 = ((fConst20 * (fConst20 + fSlow15)) + 1.0f);
		float fSlow17 = (1.0f - (fConst20 * (fSlow15 - fConst20)));
		float fSlow18 = float(knobs[2]);
		int iSlow19 = (fSlow18 > 0.0f);
		float fSlow20 = (fConst23 * std::pow(10.0f, (0.0500000007f * std::fabs(fSlow18))));
		float fSlow21 = (iSlow19 ? fConst23 : fSlow20);
		float fSlow22 = (1.0f - (fConst22 * (fSlow21 - fConst22)));
		float fSlow23 = ((fConst22 * (fConst22 + fSlow21)) + 1.0f);
		float fSlow24 = (iSlow19 ? fSlow20 : fConst23);
		float fSlow25 = ((fConst22 * (fConst22 + fSlow24)) + 1.0f);
		float fSlow26 = (1.0f - (fConst22 * (fSlow24 - fConst22)));
		float fSlow27 = float(knobs[3]);
		int iSlow28 = (fSlow27 > 0.0f);
		float fSlow29 = (fConst25 * std::pow(10.0f, (0.0500000007f * std::fabs(fSlow27))));
		float fSlow30 = (iSlow28 ? fConst25 : fSlow29);
		float fSlow31 = (1.0f - (fConst24 * (fSlow30 - fConst24)));
		float fSlow32 = ((fConst24 * (fConst24 + fSlow30)) + 1.0f);
		float fSlow33 = (iSlow28 ? fSlow29 : fConst25);
		float fSlow34 = ((fConst24 * (fConst24 + fSlow33)) + 1.0f);
		float fSlow35 = (1.0f - (fConst24 * (fSlow33 - fConst24)));
		float fSlow36 = float(knobs[4]);
		int iSlow37 = (fSlow36 > 0.0f);
		float fSlow38 = (fConst27 * std::pow(10.0f, (0.0500000007f * std::fabs(fSlow36))));
		float fSlow39 = (iSlow37 ? fConst27 : fSlow38);
		float fSlow40 = (1.0f - (fConst26 * (fSlow39 - fConst26)));
		float fSlow41 = ((fConst26 * (fConst26 + fSlow39)) + 1.0f);
		float fSlow42 = (iSlow37 ? fSlow38 : fConst27);
		float fSlow43 = ((fConst26 * (fConst26 + fSlow42)) + 1.0f);
		float fSlow44 = (1.0f - (fConst26 * (fSlow42 - fConst26)));
		float fSlow45 = float(knobs[5]);
		int iSlow46 = (fSlow45 > 0.0f);
		float fSlow47 = (fConst29 * std::pow(10.0f, (0.0500000007f * std::fabs(fSlow45))));
		float fSlow48 = (iSlow46 ? fConst29 : fSlow47);
		float fSlow49 = (1.0f - (fConst28 * (fSlow48 - fConst28)));
		float fSlow50 = ((fConst28 * (fConst28 + fSlow48)) + 1.0f);
		float fSlow51 = (iSlow46 ? fSlow47 : fConst29);
		float fSlow52 = ((fConst28 * (fConst28 + fSlow51)) + 1.0f);
		float fSlow53 = (1.0f - (fConst28 * (fSlow51 - fConst28)));
		for (int i0 = 0; (i0 < count); i0 = (i0 + 1)) {
			float fTemp0 = (fConst17 * fRec5[1]);
			fRec5[0] = (float(input0[i0]) - (((fRec5[2] * fSlow4) + fTemp0) / fSlow5));
			float fTemp1 = (fConst15 * fRec4[1]);
			fRec4[0] = ((((fTemp0 + (fRec5[0] * fSlow7)) + (fRec5[2] * fSlow8)) / fSlow5) - (((fRec4[2] * fSlow13) + fTemp1) / fSlow14));
			float fTemp2 = (fConst12 * fRec3[1]);
			fRec3[0] = ((((fTemp1 + (fRec4[0] * fSlow16)) + (fRec4[2] * fSlow17)) / fSlow14) - (((fRec3[2] * fSlow22) + fTemp2) / fSlow23));
			float fTemp3 = (fConst9 * fRec2[1]);
			fRec2[0] = ((((fTemp2 + (fRec3[0] * fSlow25)) + (fRec3[2] * fSlow26)) / fSlow23) - (((fRec2[2] * fSlow31) + fTemp3) / fSlow32));
			float fTemp4 = (fConst6 * fRec1[1]);
			fRec1[0] = ((((fTemp3 + (fRec2[0] * fSlow34)) + (fRec2[2] * fSlow35)) / fSlow32) - (((fRec1[2] * fSlow40) + fTemp4) / fSlow41));
			float fTemp5 = (fConst3 * fRec0[1]);
			fRec0[0] = ((((fTemp4 + (fRec1[0] * fSlow43)) + (fRec1[2] * fSlow44)) / fSlow41) - (((fRec0[2] * fSlow49) + fTemp5) / fSlow50));
			output0[i0] = FAUSTFLOAT((((fTemp5 + (fRec0[0] * fSlow52)) + (fRec0[2] * fSlow53)) / fSlow50));
			fRec5[2] = fRec5[1];
			fRec5[1] = fRec5[0];
			fRec4[2] = fRec4[1];
			fRec4[1] = fRec4[0];
			fRec3[2] = fRec3[1];
			fRec3[1] = fRec3[0];
			fRec2[2] = fRec2[1];
			fRec2[1] = fRec2[0];
			fRec1[2] = fRec1[1];
			fRec1[1] = fRec1[0];
			fRec0[2] = fRec0[1];
			fRec0[1] = fRec0[0];
		}
	}

};
	extern "C" {
	dsp * create() {
		return new spectrometer;
	}
}

#endif
