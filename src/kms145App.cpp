#include "kms145App.h"

//TODO Readme & upload arduino sketch
//TODO gui for serial setup - not showing the arduino
//TODO debug draw for binEq vs non filtered
//TODO different bang modes
//TODO auto bin resize
//TODO smart auto gain - the other way around?
//TODO bin settings
//TODO frequence range
//example 512 bins for 44100 -> 86Hz per bin
//1,2,4,8,16,32,64 => 127 >= 10000
//TODO draw independent of buffersize
//TODO eq on fft vs eq on bins?!

int baud = 57600;

bool bDrawGui = true;
int maxWireCount = 10;

bool bSendSerial = false;

bool eBang = false;
bool bBang = false;
bool eStupidDownGain = false;

string jsonString = "";

bool onUpdate = false;
bool eInitRequest = false;

void kms145App::setup() {
	plotHeight = 128;
	bufferSize = 1024; //TODO streamGui bufferSize changed?!

	fft = ofxFft::create(bufferSize, OF_FFT_WINDOW_HAMMING); //TODO which mode is best for us?

	audioInput = new float[bufferSize];
	fftOutput = new float[fft->getBinSize()];
	ofLogNotice() << ofToString(fft->getBinSize());

	smoothedOutput.resize(maxWireCount);
	output.resize(maxWireCount);

	setupGui();

	mode = MIC;
	appWidth = ofGetWidth();
	appHeight = ofGetHeight();

//	stream.setDeviceID(4);
	ofSoundStreamListDevices();
//	auto devices = stream.getMatchingDevices("default");
//	if(!devices.empty()){
//		stream.setDeviceID(devices[0].deviceID);
//	}
//	stream.setup(this, 0, 1, 44100, bufferSize, 4);

	serial.listDevices();
//	serial.setup("/dev/ttyACM0", baud);

	ofBackground(0, 0, 0);

	tLastDetection = ofGetElapsedTimeMillis();


	//web socket server
	ofxLibwebsockets::ServerOptions options = ofxLibwebsockets::defaultServerOptions();
	options.port = 9092;
	options.bUseSSL = false; // you'll have to manually accept this self-signed cert if 'true'!
	bSetup = server.setup( options );
	server.addListener(this);

	//video server
	ofx::HTTP::SimpleIPVideoServerSettings settings;

	// Many other settings are available.
	settings.setPort(7890);
	// The default maximum number of client connections is 5.
	settings.ipVideoRouteSettings.setMaxClientConnections(1);
	// Apply the settings.
	videoServer.setup(settings);
	// Start the server.
	videoServer.start();
	screenShot.allocate(grabScreenWidth,grabScreenHeight,OF_IMAGE_COLOR);
}

void kms145App::setupGui(){
	gui.setup("gui","settings.xml",50,50);
	gui.setSize(400,0);screenShot.allocate(grabScreenWidth,grabScreenHeight,OF_IMAGE_COLOR);
	gui.add(streamGui.setup("soundInput",&stream,this));
	gui.add(serialGui.setup("serial",&serial,this));
	videoStreamGroup.setName("video stream");
	videoStreamGroup.add(bVideoStream.set("stream it",true));
	videoStreamGroup.add(grabScreenWidth.set("width",500,100,1024));
	videoStreamGroup.add(grabScreenHeight.set("height",400,100,768));
	gui.add(videoStreamGroup);
	bangDetect.setName("bangDetect");
	bangDetect.add(onsetDelay.set("onsetDelay",100,0,2500));
	bangDetect.add(decayRate.set("decayRate",0.5,0.01,0.3));
	bangDetect.add(minimumThreshold.set("minThreshold",0.1,0,1));
	bangDetect.add(bangTime.set("bangTime",100,0,500));
	gui.add(bangDetect);
	autoGain.setName("autoGain");
	autoGain.add(bAutoGain.set("useAutoGain",true));
	autoGain.add(gain.set("gain",1,0,20));
	autoGain.add(minGain.set("minGain",1,0,20));
	autoGain.add(limit.set("limit",1,0,1));
	gui.add(autoGain);
	general.setName("general");
	general.add(binRange.set("binRange",10,1,fft->getBinSize()/4.f));
	general.add(wireCount.set("wireCount",2,1,maxWireCount));
	general.add(bUseAvg.set("useAvg",true));
	general.add(smoothFactor.set("smoothFactor",0.1,0.01,0.5));
	gui.add(general);
	eqGroup.setName("binEQ");
	eqGroup.add(bUseBinEq.set("useBinEqs",true));
	binEqs.resize(7);
	for(int i=0;i<(int)binEqs.size();++i){
		eqGroup.add(binEqs[i].set("binEq"+ofToString(i),i/(float)binEqs.size(),0,1));
	}
	gui.add(eqGroup);
	binSizesGroup.setName("binSizes");
	binSizesGroup.add(bUseCustomBinSizes.set("useCustomSizes",false));
	binSizes.resize(7);
	for(int i=0;i<(int)binSizes.size();++i){
		binSizesGroup.add(binSizes[i].set("binSize"+ofToString(i),2,1,128));
	}
	gui.add(binSizesGroup);
	gui.setWidthElements(400);
	gui.loadFromFile("settings.xml");

	paramSync.setupFromGui(gui);
	ofAddListener(paramSync.paramChangedE,this,&kms145App::parameterChanged);
//	grabScreenWidth.addListener(this,&kms145App::onSizeChanged);
//	grabScreenHeight.addListener(this,&kms145App::onSizeChanged);
//	ofAddListener(videoStreamGroup.parameterChangedE(),&kms145App::onSizeChanged);
}

void kms145App::parameterChanged( std::string & paramAsJsonString ){
	ofLogVerbose("kms145App::parameterChanged");
	if(!onUpdate)
		server.send( paramAsJsonString );
}

void kms145App::onSizeChanged(ofAbstractParameter &param){
	bool videoStreamActive = bVideoStream;
	bVideoStream = false;
	screenShot.allocate(grabScreenWidth,grabScreenHeight,OF_IMAGE_COLOR);
	bVideoStream = videoStreamActive;
}

void kms145App::update() {
	//sound device gui
	streamGui.update();

	//simple version of a auto gain TODO
	if(bAutoGain){
		if(eStupidDownGain){
			eStupidDownGain = false;
			gain = gain * 0.90;
		}else{
			gain = gain * 1.00002;
		}
		gain = gain < minGain ? (float)minGain : (float)gain;
	}

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
	if(bSendSerial){
		for(int i=0;i<wireCount;++i){
			if(bBang){
				serial.writeByte((char)255);
			}else{
				char byte = smoothedOutput[i]*256.f;
				serial.writeByte(byte);
			}
		}
	}

	//webUi
	if(eInitRequest){
		eInitRequest = false;
		jsonString = paramSync.parseParamsToJson();
		ofLogNotice("kms145App::update") << "parsed json string:" << jsonString;
		server.send(jsonString);
	}

	if(onUpdate){
		paramSync.updateParamFromJson(paramUpdate);
		onUpdate = false;
	}
}

void kms145App::draw() {
	ofSetColor(255);
	ofPushMatrix();

	//draw audio input
	ofTranslate(16, 16, 0);
	ofDrawBitmapString("Audio Input", 0, 0);
	plot(audioInput, bufferSize, plotHeight / 2, 0);

	//draw FFT of audio input
	ofTranslate(0, plotHeight + 16, 0);
	ofPushMatrix();
	ofScale(2,1);
	ofDrawBitmapString("FFT Output", 0, 0);
	plot(fftOutput, fft->getBinSize(), -plotHeight, plotHeight / 2);
	drawRedLine();
	ofPopMatrix();

	//draw processed data (AVG bins)
	ofTranslate(0, (plotHeight + 16), 0);
	ofPushStyle();
	ofScale(2,2);
	ofDrawBitmapString("Ext Plot", 0, 0);
	plotOutput(fft->getBinSize(), -plotHeight, plotHeight / 2);
	drawRedLine();
	ofPopStyle();

	//draw volume & bang threshold
	ofPushMatrix();
	ofTranslate(fft->getBinSize(), 0, 0);
	ofDrawBitmapString("Bang", 0, 0);
	plotVolume(-plotHeight, plotHeight / 2);
	ofPopMatrix();

	ofPopMatrix();
	string msg = ofToString((int) ofGetFrameRate()) + " fps";
	ofDrawBitmapString(msg, appWidth - 80, appHeight - 20);

	screenShot.grabScreen(0,0,grabScreenWidth,grabScreenHeight);
	videoServer.send(screenShot.getPixels());

	if(bDrawGui)
		gui.draw();
}

void kms145App::drawRedLine(){
	ofPushStyle();
	ofSetColor(255,0,0);
	ofDrawLine(wireCount*binRange,0,wireCount*binRange,plotHeight);
	ofPopStyle();
}

void kms145App::plot(float* array, int length, float scale, float offset) {
	ofSetColor(255);
	ofNoFill();
	ofDrawRectangle(0, 0, length, plotHeight);
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
//	ofDrawRectangle(0, 0, width, plotHeight);
	glPushMatrix();
	glTranslatef(0, plotHeight / 2 + offset, 0);
	float nextX = 0;
	for (int i = 0; i < wireCount; ++i){
		float range = binRange;
		if(bUseCustomBinSizes){
			range = binSizes[i];
		}
		ofDrawRectangle(nextX,0,range,smoothedOutput[i]*scale);
		nextX += range;
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
	ofDrawLine(0,y,50,y);
	ofFill();
	float volY = ofMap(currVol,0,0.4,0,scale);
	ofSetColor(255);
	ofDrawRectangle(0,0,50,volY);
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

	long tNow = ofGetElapsedTimeMillis();
	if(tNow-tLastDetection > onsetDelay)
		threshold = ofLerp(threshold, minimumThreshold, decayRate);

    if(rms > threshold) {
        // onset detected!
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

	setOutput(fftOutput);
}

void kms145App::setOutput(float * array){
	//TODO mutex?
	int length = fft->getBinSize();
	int lastBinIdx = 0;
	for(int wireIdx=0;wireIdx<wireCount;++wireIdx){
//	for (int i = 0; i+binRange < length && i<wireCount*binRange; i+=binRange){
		int range = 1;
		if(bUseCustomBinSizes){
			range = binSizes[wireIdx];
		}else{
			range = binRange;
		}

		float sum=0;
		for(int j=0;j<range;++j){
			sum += array[lastBinIdx+j]*gain;
		}

		if(bUseAvg){
			sum /= (float)range;
		}

		output[wireIdx] = sum;
		smoothedOutput[wireIdx] += (output[wireIdx]  - smoothedOutput[wireIdx]) * smoothFactor;

		if(bUseBinEq && wireIdx < (int)binEqs.size()){
			smoothedOutput[wireIdx] *= binEqs[wireIdx];
		}

		if(smoothedOutput[wireIdx] > limit){
			smoothedOutput[wireIdx] = limit;
			eStupidDownGain = true;
		}

		lastBinIdx += range;
	}
}

//--------------------------------------------------------------
void kms145App::onMessage( ofxLibwebsockets::Event& args ){
    ofLogVerbose("kms145App::onMessage");

    // trace out string messages or JSON messages!
    if ( !args.json.isNull() ){
    	ofLogVerbose("kms145App::onMessage") << "json message: " << args.json.toStyledString() << " from " << args.conn.getClientName();

        if(args.json["type"]=="initRequest"){
        	eInitRequest = true;
        }else if(!onUpdate){
			paramUpdate = args.json;
			onUpdate = true;
        }
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
		break;
	case 'j':
    	eInitRequest = true;
    	break;
    }
}
