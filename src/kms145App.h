#pragma once

#include "ofxSyncedParams.h"
#include "ofMain.h"
#include "ofxFft.h"
#include "ofxLibwebsockets.h"
#include "ofxSoundStreamGui.h"
#include "ofxSerialGui.h"
#include "ofxHTTP.h"

#define MIC 0
#define NOISE 1
#define SINE 2

class kms145App : public ofBaseApp {
public:
	void setup();
	void setupGui();

	void parameterChanged( std::string & );

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

	float appWidth;
	float appHeight;

	int mode;

	//----
	ofxPanel gui;
	ofParameterGroup bangDetect,autoGain,general, rootGroup, eqGroup;
	ofParameter<int> binRange, wireCount, onsetDelay, bangTime;
	ofParameter<float> minGain, gain, smoothFactor, limit, decayRate, minimumThreshold;
	ofParameter<bool> bUseAvg, bAutoGain, bUseBinEq;
	vector< ofParameter<float> > binEqs;

	ofParameterGroup videoStreamGroup;
	ofParameter<bool> bVideoStream;
	ofParameter<float> grabScreenWidth, grabScreenHeight;
	void onSizeChanged(ofAbstractParameter & param);

	ofParameterGroup binSizesGroup;
	ofParameter<bool> bUseCustomBinSizes;
	vector< ofParameter<int> > binSizes;

	ofxSyncedParams paramSync;

	ofxSoundStreamGui streamGui;
	ofxSerialGui serialGui;

	//onset detection
	float threshold, currVol;
	long tLastDetection, tBangStart;

	vector<float> smoothedOutput;
	vector<float> output;

	//--- Serial
	ofSerial serial;

	//--- WebSocket
    ofxLibwebsockets::Server server;
    ofxJSONElement paramUpdate;
    bool bSetup;

    // websocket methods
	void onMessage( ofxLibwebsockets::Event& args );
	void onConnect( ofxLibwebsockets::Event& args ){}
	void onOpen( ofxLibwebsockets::Event& args ){}
	void onClose( ofxLibwebsockets::Event& args ){}
	void onIdle( ofxLibwebsockets::Event& args ){}
	void onBroadcast( ofxLibwebsockets::Event& args ){}

	// remote video
	ofx::HTTP::SimpleIPVideoServer videoServer;
	ofImage screenShot;
};
