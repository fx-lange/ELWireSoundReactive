#include "kms145App.h"
#include "ofMain.h"
#include "ofAppGlutWindow.h"

int main() {
	ofSetupOpenGL(1024,768, OF_WINDOW);
	ofRunApp(new kms145App());
}
