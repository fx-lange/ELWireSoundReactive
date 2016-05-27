#include "kms145App.h"
#include "ofMain.h"
#include "ofAppGlutWindow.h"

int main() {
	ofAppGlutWindow window;
	ofSetupOpenGL(&window, 800 + 32, (128 + 32) * 4, OF_WINDOW);
	ofRunApp(new kms145App());
}
