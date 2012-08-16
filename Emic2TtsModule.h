// --------------------------------------------------------------------------------
// An Arduino library for communicating with the Emic 2 Text-to-Speech Module.
//
// MIT license.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//      **********************************************************************
//      Designed for the Emic 2 Text-to-Speech Module, developed by
//        Grand Idea Studio and Parallax:
//      http://www.grandideastudio.com/portfolio/emic-2-text-to-speech-module/
//
//      You can pick one up in the Adafruit Store:
//      http://www.adafruit.com/products/924
//      **********************************************************************
//
// --------------------------------------------------------------------------------
#ifndef Emic2TtsModule_h
#define Emic2TtsModule_h

#include <Arduino.h>
#include <SoftwareSerial.h>

// Emic 2 voice values based on the module notes 
enum EmicVoice {
  PerfectPaul = 0,
  HugeHarry = 1,
  BeautifulBetty = 2,
  UppityUrsula = 3,
  DoctorDennis = 4,
  KitTheKid = 5,
  FrailFrank = 6,
  RoughRita = 7,
  WhisperingWendy = 8
};

// Emic 2 language values based on the module notes 
enum EmicLanguage {
  English = 0,
  CastilianSpanish = 1,
  LatinSpanish = 2
};

// Emic 2 parser values based on the module notes 
enum EmicParser {
  DECtalk = 0,
  Epson = 1
};

class Emic2TtsModule {
  
  public:
    Emic2TtsModule(SoftwareSerial* serialPort);

    void init();

    void playSpeakingDemo();
    void playSingingDemo();
    void playSpanishDemo();
    
    void setVolume(int volume);
    void setVoice(EmicVoice voice);
    void setWordsPerMinute(int rate);
    void setLanguage(EmicLanguage language);
    void setParser(EmicParser parser);
    void restoreDefaults();
    
    void say(const __FlashStringHelper *);
    void say(const String &);
    void say(const char[]);
    void say(char);
    void say(unsigned char, int = DEC);
    void say(int, int = DEC);
    void say(unsigned int, int = DEC);
    void say(long, int = DEC);
    void say(unsigned long, int = DEC);
    void say(double, int = 2);
    void say(const Printable&);

    void sendCommand(char command);
    void sendCommand(char command, int param);
    
  private:
    void sendTerminatorAndWait();
    SoftwareSerial* _serialPort;
};

#endif // Emic2TtsModule_h
