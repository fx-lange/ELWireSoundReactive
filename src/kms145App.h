#pragma once

#include "ofMain.h"
#include "ofxFft.h"
#include "ofxGui.h"

#define MIC 0
#define NOISE 1
#define SINE 2

class kms145App : public ofBaseApp {
public:
	void setup();
	void setupGui();
	void update();
	void setOutput(float * arrays);
	void plot(float* array, int length, float scale, float offset);
	void plotOutput(float width, float scale, float offset);
	void plotVolume(float scale, float offset);
	void audioReceived(float* input, int bufferSize, int nChannels);
	void draw();
	void drawRedLine();
	void keyPressed(int key);

	int plotHeight, bufferSize;

	ofxFft* fft;
	ofSoundStream stream;

	float* audioInput;
	float* fftOutput;
	float* eqFunction;
	float* eqOutput;
	float* ifftOutput;

	float appWidth;
	float appHeight;

	int mode;

	//----
	ofxPanel gui;
	ofxIntSlider binRange, wireCount;
	ofxFloatSlider minGain, gain, smoothFactor, limit;
	ofxToggle bUseAvg, bUseFilter;

	//onset detection
	ofxFloatSlider decayRate, minimumThreshold;
	ofxIntSlider onsetDelay, bangTime;
	float threshold, currVol;
	long tLastDetection, tBangStart;

	vector<float> smoothedOutput;
	vector<float> output;

	//-----
	ofSerial serial;
};
