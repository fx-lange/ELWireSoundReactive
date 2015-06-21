#include "kms145App.h"

//TODO auto bin resize
//TODO smart auto gain
//TODO logarit output
//TODO eq by dragging
//Todo bin settings
//todo frequence range
//TODO usb sound card
//TODO gui for serial & sound setup
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
	bufferSize = 1024;

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

	stream.setDeviceID(4);
	stream.setup(this, 0, 1, 44100, bufferSize, 4);
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
	gui.setup("gui","settings.xml",bufferSize+25,25);
	bangDetect.setName("bangDetect");
	bangDetect.add(onsetDelay.set("onsetDelay",100,0,2500));
	bangDetect.add(decayRate.set("decayRate",0.5,0.01,0.3));
	bangDetect.add(minimumThreshold.set("minThreshold",0.1,0,1));
	bangDetect.add(bangTime.set("bangTime",100,0,500));
	rootGroup.add(bangDetect);
	autoGain.setName("autoGain");
	autoGain.add(gain.set("gain",1,0,20));
	autoGain.add(minGain.set("minGain",1,0,20));
	autoGain.add(limit.set("limit",1,0,1));
	rootGroup.add(autoGain);
	general.setName("general");
	general.add(binRange.set("binRange",10,1,fft->getBinSize()/4.f));
	general.add(wireCount.set("wireCount",2,1,maxWireCount));
	general.add(bUseAvg.set("useAvg",true));
	general.add(bUseFilter.set("useEq",false));
	general.add(smoothFactor.set("smoothFactor",0.1,0.01,0.5));
	rootGroup.add(general);
	rootGroup.setName("groups");
	gui.add(rootGroup);
	gui.loadFromFile("settings.xml");

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
	}else if(parameter.type()==typeid(ofParameter<float>).name()){
		const ofParameter<float> & p = parameter.cast<float>();
		json[ "value" ] = p.get();
		json[ "name" ] = p.getName();
	}else if(parameter.type()==typeid(ofParameter<bool>).name()){
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
	//simple version of a auto gain TODO
//	if(eStupidDownGain){
//		eStupidDownGain = false;
//		gain = gain * 0.90;
//	}else{
//		gain = gain * 1.00002;
//	}
//	gain = gain < minGain ? (float)minGain : (float)gain;

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

	//webUi
	if(eInitRequest){
		eInitRequest = false;
		jsonString = DC.setFromOfParameterGroup(((ofParameterGroup&)gui.getParameter()));
		cout << "parsed json string" << jsonString;
		server.send(jsonString);
	}

	if(onUpdate){
		ofParameterGroup & group = (ofParameterGroup&)gui.getParameter();
		DC.setParamFromJson(paramUpdate,&group);
		onUpdate = false;
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

//--------------------------------------------------------------
void kms145App::onConnect( ofxLibwebsockets::Event& args ){
    cout<<"on connected"<<endl;
}

//--------------------------------------------------------------
void kms145App::onOpen( ofxLibwebsockets::Event& args ){
    cout<<"new connection open"<<endl;
}

//--------------------------------------------------------------
void kms145App::onClose( ofxLibwebsockets::Event& args ){
    cout<<"on close"<<endl;
}

//--------------------------------------------------------------
void kms145App::onIdle( ofxLibwebsockets::Event& args ){
    cout<<"on idle"<<endl;
}

//--------------------------------------------------------------
void kms145App::onMessage( ofxLibwebsockets::Event& args ){
    cout<<"got message "<<args.message<<endl;

    // trace out string messages or JSON messages!
    if ( !args.json.isNull() ){
        cout << "New message: " << args.json.toStyledString() << " from " << args.conn.getClientName();

        if(args.json["type"]=="initRequest"){
        	eInitRequest = true;
        }else if(!onUpdate){
			paramUpdate = args.json;
			onUpdate = true;
        }
    }
}

//--------------------------------------------------------------
void kms145App::onBroadcast( ofxLibwebsockets::Event& args ){
    cout<<"got broadcast "<<args.message<<endl;
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
