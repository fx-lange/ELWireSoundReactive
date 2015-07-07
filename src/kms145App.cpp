#include "kms145App.h"

//TODO auto bin resize
//TODO smart auto gain
//TODO different bang modes
//TODO bin settings
//TODO frequence range
//TODO gui for serial setup
//TODO draw independent of buffersize

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

	smoothedOutput.resize(maxWireCount);
	output.resize(maxWireCount);

	setupGui();

	// 0 output channels,
	// 1 input channel
	// 44100 samples per second
	// [bins] samples per buffer
	// 4 num buffers (latency)

	mode = MIC;
	appWidth = ofGetWidth();
	appHeight = ofGetHeight();

//	stream.setDeviceID(4);
//	stream.setup(this, 0, 1, 44100, bufferSize, 4);
	ofSoundStreamListDevices();

	serial.listDevices();
	serial.setup("/dev/ttyACM0", baud);

	ofBackground(0, 0, 0);

	tLastDetection = ofGetElapsedTimeMillis();


	//web socket server
	ofxLibwebsockets::ServerOptions options = ofxLibwebsockets::defaultServerOptions();
	options.port = 9092;
	options.bUseSSL = false; // you'll have to manually accept this self-signed cert if 'true'!
	bSetup = server.setup( options );
	server.addListener(this);
}

void kms145App::setupGui(){
	gui.setup("gui","settings.xml",50,50);
	gui.setSize(400,0);
	gui.add(streamGui.setup("soundInput",&stream,this));
//	gui.add(serialGui.setup("serial",&serial,this));
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
	gui.setWidthElements(400);
	gui.loadFromFile("settings.xml");

	paramSync.setupFromGui(gui);
	ofAddListener(((ofParameterGroup&)gui.getParameter()).parameterChangedE,this,&kms145App::parameterChanged);
}

void kms145App::parameterChanged( ofAbstractParameter & parameter ){
	ofLogVerbose("kms145App::parameterChanged");
	if(onUpdate){
		ofLogVerbose("kms145App::parameterChanged") << "on update -> return";
		return;
	}

//	if(updatingParameter) return;
//	sender.sendParameter(parameter);
	Json::Value path;
	const vector<string> hierarchy = parameter.getGroupHierarchyNames();
	for(int i=0;i<(int)hierarchy.size()-1;i++){
		path.append(hierarchy[i]);
	}

	Json::Value json;
	json["type"] = "update";

	if(parameter.type()==typeid(ofParameter<int>).name()){
		const ofParameter<int> & p = parameter.cast<int>();
		json[ "value" ] = p.get();
		json[ "name" ] = p.getName();
	}else if(parameter.type()==typeid(ofParameter<float>).name()){
		const ofParameter<float> & p = parameter.cast<float>();
		json[ "value" ] = p.get();
		json[ "name" ] = p.getName();
	}else if(parameter.type()==typeid(ofParameter<bool>).name()){
		const ofParameter<bool> & p = parameter.cast<bool>();
		json[ "value" ] = p.get();
		json[ "name" ] = p.getName();
	}else if(parameter.type()==typeid(ofParameter<ofColor>).name()){
		const ofParameter<ofColor> & p = parameter.cast<ofColor>();

        Json::Value jsonArray;
        jsonArray.append(p.get().r);
        jsonArray.append(p.get().g);
        jsonArray.append(p.get().b);

        json[ "value" ] = jsonArray;
        json[ "name" ] = p.getName();
	}else{
	}

	json["path"] = path;
	ofxJSONElement element(json);
	ofLogNotice(element.toStyledString());

	server.send( element.toStyledString() );

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
				serial.writeByte(smoothedOutput[i]*256.f);
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
	ofDrawRectangle(0, 0, width, plotHeight);
	glPushMatrix();
	glTranslatef(0, plotHeight / 2 + offset, 0);
	for (int i = 0; i < wireCount; ++i){
		ofDrawRectangle(i*binRange,0,binRange,smoothedOutput[i]*scale);
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

		if(bUseBinEq && wireIdx < (int)binEqs.size()){
			smoothedOutput[wireIdx] *= binEqs[wireIdx];
		}

		if(smoothedOutput[wireIdx] > limit){
			smoothedOutput[wireIdx] = limit;
			eStupidDownGain = true;
		}
		++wireIdx;
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
