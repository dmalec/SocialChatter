// --------------------------------------------------------------------------------
// An Arduino sketch for reading tweets using the Emic 2 Text to Speech module.
// Derived/copied from the Gutenbird sketch for the Adafruit internet of things
// printer:
//      https://github.com/adafruit/Adafruit-Tweet-Receipt
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
#include <SPI.h>
#include <Ethernet.h>
#include <SoftwareSerial.h>
#include "Emic2TtsModule.h"

// Stream parse states
enum ParseState {
  STATE_NORMAL,
  STATE_LINK_H,
  STATE_LINK_HT,
  STATE_LINK_HTT,
  STATE_LINK_HTTP,
  STATE_LINK_HTTPS,
  STATE_LINK_HTTP_COLON,
  STATE_LINK_HTTP_COLON_SLASH,
  STATE_LINK_FOUND,
  STATE_LINK_FALSE_POSITIVE,
  STATE_LINK_DONE
};

// Emic 2 Globals
const int emic2RxPin = 2;
const int emic2TxPin = 3;
SoftwareSerial emic2Serial = SoftwareSerial(emic2RxPin, emic2TxPin);
Emic2TtsModule emic2TtsModule = Emic2TtsModule(&emic2Serial);

// Twitter Configuration
char *serverName  = "search.twitter.com";

// queryString can be any valid Twitter API search string, including
// boolean operators.  See https://dev.twitter.com/docs/using-search
// for options and syntax.  Funny characters do NOT need to be URL
// encoded here -- the sketch takes care of that.
char *queryString = "curiosity";
const int maxTweets = 5;
const unsigned long pollingInterval = 60L * 1000L;

// Network globals
byte macAddr[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x65, 0x92 };
IPAddress fallbackIpAddr(10, 0, 1, 201);
EthernetClient client;
const unsigned long connectTimeout  = 15L * 1000L;
const unsigned long responseTimeout = 15L * 1000L;

// Internal globals
byte resultsDepth;   // parse depth
char lastId[21];     // 18446744073709551615\0 (64-bit maxint as string)
char fromUser[16];   // Max username length (15) + \0
char msgText[141];   // Max tweet length (140) + \0
char name[11];       // Temp space for name:value parsing
char value[256];     // Temp space for name:value parsing

// Function prototypes -------------------------------------------------------
boolean jsonParse(int, byte);
boolean readString(char *, int);
int writeStringIfPossible(int, int, char *, char *);
int unidecode(byte);
int timedRead(void);

void setup() {
  // Set up the Emic2 module.
  Serial.print(F("Initializing Emic..."));
  pinMode(emic2RxPin, INPUT);
  pinMode(emic2TxPin, OUTPUT);
  emic2Serial.begin(9600);
  emic2TtsModule.init();
  emic2TtsModule.setVolume(5);
  emic2TtsModule.setWordsPerMinute(120);
  emic2TtsModule.setVoice(BeautifulBetty);
  Serial.print(F("OK"));

  // Set up the Network connection.
  Serial.print(F("Initializing Ethernet..."));
  emic2TtsModule.say(F("Initializing Ethernet."));
  if(Ethernet.begin(macAddr)) {
    Serial.println(F("OK"));
    emic2TtsModule.say(F("OK."));
  } else {
    Serial.print(F("\r\nno DHCP response, using static IP address."));
    emic2TtsModule.say(F("No DHCP response, using static IP address."));
    Ethernet.begin(macAddr, fallbackIpAddr);
  }

  // Clear all string data
  memset(lastId   , 0, sizeof(lastId));
  memset(fromUser , 0, sizeof(fromUser));
  memset(msgText  , 0, sizeof(msgText));
  memset(name     , 0, sizeof(name));
  memset(value    , 0, sizeof(value));
}


void loop() {
  unsigned long startTime, t;
  int           i;
  char          c;

  startTime = millis();

  // Attempt server connection, with timeout...
  Serial.print(F("Connecting to server..."));
  while((client.connect(serverName, 80) == false) &&
    ((millis() - startTime) < connectTimeout));

  if(client.connected()) { // Success!
    Serial.print(F("OK\r\nIssuing HTTP request..."));
    // URL-encode queryString to client stream:
    client.print(F("GET /search.json?q="));
    for(i=0; c=queryString[i]; i++) {
      if(((c >= 'a') && (c <= 'z')) ||
         ((c >= 'A') && (c <= 'Z')) ||
         ((c >= '0') && (c <= '9')) ||
          (c == '-') || (c == '_')  ||
          (c == '.') || (c == '~')) {
        // Unreserved char: output directly
        client.write(c);
      } else {
        // Reserved/other: percent encode
        client.write('%');
        client.print(c, HEX);
      }
    }
    client.print(F("&result_type=recent&include_entities=false&rpp="));
    if(lastId[0]) {
      client.print(maxTweets);    // Limit to avoid runaway talking
      client.print(F("&since_id=")); // Display tweets since prior query
      client.print(lastId);
    } else {
      client.print('1'); // First run; announce single latest tweet
    }
    client.print(F(" HTTP/1.1\r\nHost: "));
    client.println(serverName);
    client.println(F("Connection: close\r\n"));

    Serial.print(F("OK\r\nAwaiting results (if any)..."));
    t = millis();
    while((!client.available()) && ((millis() - t) < responseTimeout));
    if(client.available()) { // Response received?
      // Could add HTTP response header parsing here (400, etc.)
      if(client.find("\r\n\r\n")) { // Skip HTTP response header
        Serial.println(F("OK\r\nProcessing results..."));
        resultsDepth = 0;
        jsonParse(0, 0);
      } else Serial.println(F("response not recognized."));
    } else   Serial.println(F("connection timed out."));
    client.stop();
  } else { // Couldn't contact server
    Serial.println(F("failed"));
  }

  // Pause between queries, factoring in time already spent on network
  // access, parsing, printing above.
  t = millis() - startTime;
  if(t < pollingInterval) {
    Serial.print(F("Pausing..."));
    delay(pollingInterval - t);
    Serial.println(F("done"));
  }
}

// ---------------------------------------------------------------------------

boolean jsonParse(int depth, byte endChar) {
  int     c, i;
  boolean readName = true;

  for(;;) {
    while(isspace(c = timedRead())); // Scan past whitespace
    if(c < 0)        return false;   // Timeout
    if(c == endChar) return true;    // EOD

    if(c == '{') { // Object follows
      if(!jsonParse(depth + 1, '}')) return false;
      if(!depth)                     return true; // End of file
      if(depth == resultsDepth) { // End of object in results list

        // Output to Emic 2
        emic2TtsModule.say(fromUser);
        emic2TtsModule.say(F(" tweeted "));
        emic2TtsModule.say(msgText);

        // Dump to serial console as well
        Serial.print("User: ");
        Serial.println(fromUser);
        Serial.print("Text: ");
        Serial.println(msgText);

        // Clear strings for next object
        fromUser[0] = msgText[0] = 0;
      }
    } else if(c == '[') { // Array follows
      if((!resultsDepth) && (!strcasecmp(name, "results")))
        resultsDepth = depth + 1;
      if(!jsonParse(depth + 1,']')) return false;
    } else if(c == '"') { // String follows
      if(readName) { // Name-reading mode
        if(!readString(name, sizeof(name)-1)) return false;
      } else { // Value-reading mode
        if(!readString(value, sizeof(value)-1)) return false;
        // Process name and value strings:
        if       (!strcasecmp(name, "max_id_str")) {
          strncpy(lastId, value, sizeof(lastId)-1);
        } else if(!strcasecmp(name, "from_user")) {
          strncpy(fromUser, value, sizeof(fromUser)-1);
        } else if(!strcasecmp(name, "text")) {
          strncpy(msgText, value, sizeof(msgText)-1);
        }
      }
    } else if(c == ':') { // Separator between name:value
      readName = false; // Now in value-reading mode
      value[0] = 0;     // Clear existing value data
    } else if(c == ',') {
      // Separator between name:value pairs.
      readName = true; // Now in name-reading mode
      name[0]  = 0;    // Clear existing name data
    } // Else true/false/null or a number follows.  These values aren't
      // used or expected by this program, so just ignore...either a comma
      // or endChar will come along eventually, these are handled above.
  }
}

// ---------------------------------------------------------------------------

// Read string from client stream into destination buffer, up to a maximum
// requested length.  Buffer should be at least 1 byte larger than this to
// accommodate NUL terminator.  Opening quote is assumed already read,
// closing quote will be discarded, and stream will be positioned
// immediately following the closing quote (regardless whether max length
// is reached -- excess chars are discarded).  Returns true on success
// (including zero-length string), false on timeout/read error.
boolean readString(char *dest, int maxLen) {
  int c, len = 0, link_buffer_idx;
  boolean done = false, read_next_char = true;
  char link_buffer[10];
  ParseState state = STATE_NORMAL;

  while(!done) {
    // Handle cases where a character needs to be parsed twice.
    if (read_next_char) {
      c = timedRead();
    } else {
      read_next_char = true;
    }
    
    // Exit if current character is the closing quote.
    if (c == '"') {
      done = true;
      break;
    }
    
    // Handle escaped characters.
    if(c == '\\') {
      c = timedRead();
      
      // Replace control characters with a period
      if     (c == 'b') c = '.'; // Backspace
      else if(c == 'f') c = '.'; // Form feed
      else if(c == 'n') c = '.'; // Newline
      else if(c == 'r') c = '.'; // Carriage return
      else if(c == 't') c = '.'; // Tab
      else if(c == 'u') c = unidecode(4);
      else if(c == 'U') c = unidecode(8);
      // else c is unaltered -- an escaped char such as \ or "
    }
          
    // Special handling for http links & special characters #, :, /, (, ), *
    if (state == STATE_NORMAL) {
      if (c == 'h') {
        state = STATE_LINK_H;
        memset(link_buffer, 0, sizeof(link_buffer));
        link_buffer_idx = 0;
      } else {
        // Special handling for certain characters
        if (c == '#') {
          len = writeStringIfPossible(len, maxLen, dest, " hash \0");
        } else if (c == ':') {
          len = writeStringIfPossible(len, maxLen, dest, " colon \0");
        } else if (c == '/') {
          len = writeStringIfPossible(len, maxLen, dest, " slash \0");
        } else if (c == '(' || c == ')') {
          len = writeStringIfPossible(len, maxLen, dest, " parenthesis \0");
        } else if (c == '*') {
          len = writeStringIfPossible(len, maxLen, dest, " star \0");
        } else {
          // Handle timeout
          if (c < 0) {
            dest[len] = 0;
            return false;
          }
                   
          // In order to properly position the client stream at the end of
          // the string, characters are read to the end quote, even if the max
          // string length is reached...the extra chars are simply discarded.
          if(len < maxLen) dest[len++] = c;
        }   
      }
    } else if (state == STATE_LINK_H) {
      if (c == 't') {
        state = STATE_LINK_HT;
      } else {
        state = STATE_LINK_FALSE_POSITIVE;
      }
    } else if (state == STATE_LINK_HT) {
      if (c == 't') {
        state = STATE_LINK_HTT;
      } else {
        state = STATE_LINK_FALSE_POSITIVE;
      }
    } else if (state == STATE_LINK_HTT) {
      if (c == 'p') {
        state = STATE_LINK_HTTP;
      } else {
        state = STATE_LINK_FALSE_POSITIVE;
      }
    } else if (state == STATE_LINK_HTTP) {
      if (c == ':') {
        state = STATE_LINK_HTTP_COLON;
      } else if (c == 's') {
        state = STATE_LINK_HTTPS;
      } else {
        state = STATE_LINK_FALSE_POSITIVE;
      }
    } else if (state == STATE_LINK_HTTPS) {
      if (c == ':') {
        state = STATE_LINK_HTTP_COLON;
      } else {
        state = STATE_LINK_FALSE_POSITIVE;
      }
    } else if (state == STATE_LINK_HTTP_COLON) {
      if (c == '/') {
        state = STATE_LINK_HTTP_COLON_SLASH;
      } else {
        state = STATE_LINK_FALSE_POSITIVE;
      }
    } else if (state == STATE_LINK_HTTP_COLON_SLASH) {
      if (c == '/') {
        state = STATE_LINK_FOUND;
      } else {
        state = STATE_LINK_FALSE_POSITIVE;
      }
    } else if (state == STATE_LINK_FOUND) {
      if (c != '.' && c != '/' && !isalnum(c)) {
        state = STATE_LINK_DONE;
      }
    }
    
    switch (state) {
      case STATE_NORMAL:
      case STATE_LINK_FOUND:
        // Do nothing.
        break;
        
      case STATE_LINK_FALSE_POSITIVE:
        len = writeStringIfPossible(len, maxLen, dest, link_buffer);
        read_next_char = false;
        state = STATE_NORMAL;
        break;
        
      case STATE_LINK_DONE:
        len = writeStringIfPossible(len, maxLen, dest, " link \0");
        read_next_char = false;
        state = STATE_NORMAL;
        break;
      
      default:
        // Save current character in case of false positive.
        link_buffer[link_buffer_idx++] = c;
        break;
    }
  }

  dest[len] = 0;
  return true; // Success (even if empty string)
}

// Write a passed in string out to the destination buffer if possible, advancing the
// length as needed and returning it to the caller.
int writeStringIfPossible(int len, int maxLen, char *dest, char *str) {
  int str_idx = 0;
  
  while (str[str_idx] != '\0' && len < maxLen) {
    dest[len++] = str[str_idx++];
  }

  return len;
}


// ---------------------------------------------------------------------------

// Read a given number of hexadecimal characters from client stream,
// representing a Unicode symbol.  Return -1 on error, else return space.
int unidecode(byte len) {
  int c, v, result = 0;
  while(len--) {
    if((c = timedRead()) < 0) return -1; // Stream timeout
  }

  // To do: some Unicode symbols may have equivalents in the ISO-8859-1 Latin
  // character set supported by the Emic 2.
  return ' ';
}

// ---------------------------------------------------------------------------

// Read from client stream with a 5 second timeout.  Although an
// essentially identical method already exists in the Stream() class,
// it's declared private there...so this is a local copy.
int timedRead(void) {
  int           c;
  unsigned long start = millis();

  while((!client.available()) && ((millis() - start) < 5000L));

  return client.read();  // -1 on timeout
}
