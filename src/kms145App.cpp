#include "kms145App.h"

//TODO auto bin resize
//TODO smart auto gain
//TODO logarit output
//TODO eq by dragging
//Todo bin settings
//todo frequence range

int baud = 57600;

bool bDrawGui = true;
int maxWireCount = 10;

bool bSendSerial = false;

bool eBang = false;
bool bBang = false;
bool eStupidDownGain = false;

void kms145App::setup() {
	plotHeight = 128;
	bufferSize = 512;

	fft = ofxFft::create(bufferSize, OF_FFT_WINDOW_HAMMING); //TODO which mode is best for us?

	audioInput = new float[bufferSize];
	fftOutput = new float[fft->getBinSize()];
	eqFunction = new float[fft->getBinSize()];
	eqOutput = new float[fft->getBinSize()];
	ifftOutput = new float[bufferSize];

	smoothedOutput.resize(maxWireCount);
	output.resize(maxWireCount);

	setupGui();

	// 0 output channels,
	// 1 input channel
	// 44100 samples per second
	// [bins] samples per buffer
	// 4 num buffers (latency)

	// this describes a linear low pass filter
	//TODO do we need some filter?
	for(int i = 0; i < fft->getBinSize(); i++){
		eqFunction[i] = (float) (fft->getBinSize() - i) / (float) fft->getBinSize();
//		eqFunction[i] = ofMap(eqFunction[i],0,1,1,025);
	}

	mode = MIC;
	appWidth = ofGetWidth();
	appHeight = ofGetHeight();

	ofSoundStreamSetup(0, 1, this, 44100, bufferSize, 4);
	ofSoundStreamListDevices();

	serial.listDevices();
	serial.setup("/dev/ttyACM0", baud);

	ofBackground(0, 0, 0);

	tLastDetection = ofGetElapsedTimeMillis();
}

void kms145App::setupGui(){
	gui.setup("gui","settings.xml",bufferSize+25,25);
	gui.add(binRange.setup("bin range",10,1,fft->getBinSize()/4.f));
	gui.add(wireCount.setup("wire count",2,1,maxWireCount));
	gui.add(bUseAvg.setup("use avg",true));
	gui.add(bUseFilter.setup("use eq",false));
	gui.add(gain.setup("gain",1,0,20));
	gui.add(minGain.setup("min gain",1,0,20));
	gui.add(limit.setup("limit",1,0,1));
	gui.add(smoothFactor.setup("smooth factor",0.1,0.01,0.5));
	gui.add(onsetDelay.setup("onset delay",100,0,2500));
	gui.add(decayRate.setup("decay rate",0.5,0.01,0.3));
	gui.add(minimumThreshold.setup("min threshold",0.1,0,1));
	gui.add(bangTime.setup("bang time",100,0,500));
	gui.loadFromFile("settings.xml");
}

void kms145App::update() {
	//simple version of a auto gain TODO
	if(eStupidDownGain){
		eStupidDownGain = false;
		gain = gain * 0.90;
	}else{
		gain = gain * 1.00002;
	}
	gain = gain < minGain ? (float)minGain : (float)gain;

	//onset detection
	long tNow = ofGetElapsedTimeMillis();
	if(eBang){
		eBang = false;
		bBang = true;
		tBangStart = tNow;
	}

	if(bBang){
		ofBackground(100);
		if(tNow - tBangStart > bangTime){
			bBang = false;
		}
	}else{
		ofBackground(0);
	}

	//serial communication
	//TODO linear interpolation between 0-255 doesn't really fit the brightness curve of El Wires
	if(bSendSerial){
		for(int i=0;i<wireCount;++i){
			if(bBang){
				serial.writeByte((char)255);
			}else{
				serial.writeByte((char)(smoothedOutput[i]*256.f));
			}
		}
	}
}

void kms145App::draw() {
	ofSetColor(255);
	ofPushMatrix();

	glTranslatef(16, 16, 0);
	ofDrawBitmapString("Audio Input", 0, 0);
	plot(audioInput, bufferSize, plotHeight / 2, 0);

	glTranslatef(0, plotHeight + 16, 0);
	ofDrawBitmapString("FFT Output", 0, 0);
	plot(fftOutput, fft->getBinSize(), -plotHeight, plotHeight / 2);
	drawRedLine();

	ofPushMatrix();
	glTranslatef(fft->getBinSize(), 0, 0);
	ofDrawBitmapString("EQd FFT Output", 0, 0);
	plot(eqOutput, fft->getBinSize(), -plotHeight, plotHeight / 2);
	drawRedLine();
	ofPopMatrix();

//	glTranslatef(0, plotHeight + 16, 0);
//	ofDrawBitmapString("IFFT Output", 0, 0);
//	plot(ifftOutput, fft->getSignalSize(), plotHeight / 2, 0);

	glTranslatef(0, (plotHeight + 16), 0);
	ofPushStyle();
	ofScale(2,2);
	ofDrawBitmapString("Ext Plot", 0, 0);
	plotOutput(fft->getBinSize(), -plotHeight, plotHeight / 2);
	drawRedLine();
	ofPopStyle();

	ofPushMatrix();
	glTranslatef(fft->getBinSize(), 0, 0);
	ofDrawBitmapString("Bang", 0, 0);
	plotVolume(-plotHeight, plotHeight / 2);
	ofPopMatrix();

	ofPopMatrix();
	string msg = ofToString((int) ofGetFrameRate()) + " fps";
	ofDrawBitmapString(msg, appWidth - 80, appHeight - 20);

	if(bDrawGui)
		gui.draw();
}

void kms145App::drawRedLine(){
	ofPushStyle();
	ofSetColor(255,0,0);
	ofLine(wireCount*binRange,0,wireCount*binRange,plotHeight);
	ofPopStyle();
}

void kms145App::plot(float* array, int length, float scale, float offset) {
	ofSetColor(255);
	ofNoFill();
	ofRect(0, 0, length, plotHeight);
	glPushMatrix();
	glTranslatef(0, plotHeight / 2 + offset, 0);
	ofBeginShape();
	for (int i = 0; i < length; i++)
		ofVertex(i, array[i] * scale);
	ofEndShape();
	glPopMatrix();
}

void kms145App::plotOutput(float width, float scale, float offset) {
	ofNoFill();
	ofRect(0, 0, width, plotHeight);
	glPushMatrix();
	glTranslatef(0, plotHeight / 2 + offset, 0);
	for (int i = 0; i < wireCount; ++i){
		ofRect(i*binRange,0,binRange,smoothedOutput[i]*scale);
	}
	glPopMatrix();
}

void kms145App::plotVolume(float scale, float offset){
	ofNoFill();
//	ofRect(0,0,50,plotHeight);
	glPushMatrix();
	glTranslatef(0, plotHeight / 2 + offset, 0);
	float y = ofMap(threshold,0,0.4,0,scale);
	ofSetColor(255,0,0);
	ofLine(0,y,50,y);
	ofFill();
	float volY = ofMap(currVol,0,0.4,0,scale);
	ofSetColor(255);
	ofRect(0,0,50,volY);
	glPopMatrix();
}

void kms145App::audioReceived(float* input, int bufferSize, int nChannels) {
	// modified from audioInputExample
	float rms = 0.0;
	int numCounted = 0;

	for (int i = 0; i < bufferSize; i++){
	    rms += input[i] * input[i];
	    numCounted ++;
	}

	rms /= (float)numCounted;
	rms = sqrt(rms);
	currVol = rms;

//	cout << rms << " " << threshold << endl;

	long tNow = ofGetElapsedTimeMillis();
	if(tNow-tLastDetection > onsetDelay)
		threshold = ofLerp(threshold, minimumThreshold, decayRate);

    if(rms > threshold) {
        // onset detected!
    	cout << "ONSET" << endl;
    	tLastDetection = tNow;
        threshold = rms;
        eBang = true;
    }

    //input -> fft
	if (mode == MIC) {
		// store input in audioInput buffer
		memcpy(audioInput, input, sizeof(float) * bufferSize);
	} else if (mode == NOISE) {
		for (int i = 0; i < bufferSize; i++)
			audioInput[i] = ofRandom(-1, 1);
	} else if (mode == SINE) {
		for (int i = 0; i < bufferSize; i++)
			audioInput[i] = sinf(PI * i * mouseX / appWidth);
	}

	fft->setSignal(audioInput);
	memcpy(fftOutput, fft->getAmplitude(), sizeof(float) * fft->getBinSize());

	for(int i = 0; i < fft->getBinSize(); i++)
		eqOutput[i] = fftOutput[i] * eqFunction[i];

	if(bUseFilter){
		setOutput(eqOutput);
	}else{
		setOutput(fftOutput);
	}

	fft->setPolar(eqOutput, fft->getPhase());

	fft->clampSignal();
	memcpy(ifftOutput, fft->getSignal(), sizeof(float) * fft->getSignalSize());
}

void kms145App::setOutput(float * array){
	//TODO mutex?
	int length = fft->getBinSize();
	int wireIdx = 0;
	for (int i = 0; i+binRange < length && i<wireCount*binRange; i+=binRange){
		float sum=0;
		for(int j=0;j<binRange;++j){
			sum += array[i+j]*gain;
		}
		if(bUseAvg)
			sum /= (float)binRange;
		output[wireIdx] = sum;
		smoothedOutput[wireIdx] += (output[wireIdx]  - smoothedOutput[wireIdx]) * smoothFactor;
		if(smoothedOutput[wireIdx] > limit){
			smoothedOutput[wireIdx] = limit;
			eStupidDownGain = true;
		}
		++wireIdx;
	}
}

void kms145App::keyPressed(int key) {
	switch (key) {
	case 'm':
		mode = MIC;
		break;
	case 'n':
		mode = NOISE;
		break;
	case 's':
		mode = SINE;
		break;
	case 'g':
		bDrawGui = !bDrawGui;
		break;
	case 'c':
		bSendSerial = !bSendSerial;
	}
}
