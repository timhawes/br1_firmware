/*
 * br1_firmware - for http://timhawes.com/br1 PCB
 *
 * Arduino packages:
 *   https://github.com/esp8266/Arduino/
 *
 * Libraries:
 *   https://github.com/adafruit/Adafruit_NeoPixel/
 *
 */

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))


#ifdef ESP8266
extern "C" {
#include "user_interface.h"
}
#endif

// #define DEBUG
// #define DEBUG_PACKETS

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTDEC(x) Serial.print(x, DEC)
#define DEBUG_PRINTHEX(x) Serial.print(x, HEX)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTHEX(x)
#define DEBUG_PRINTLN(x)
#endif

#define MAX_PIXELS 1024

const uint8_t irPin = 14;
const uint8_t buttonPin = 13;
const uint8_t dataPin = 12;
const unsigned int udpPort = 2812;

struct EepromData {
  uint8_t configured;
  char ssid[128];
  char passphrase[128];
  uint16_t pixelcount;
  uint16_t colourorder;
  uint8_t scalered;
  uint8_t scalegreen;
  uint8_t scaleblue;
} eepromData;

char myhostname[64];
IPAddress ip;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel();
ESP8266WebServer server(80);
WiFiUDP Udp;
uint8_t inboundMessage[1500];
uint8_t ledMode = 9;
boolean ledModeChanged = false;
uint8_t buttonState = HIGH;

void setup() {

  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting - Hold button to enter configuration mode");

  snprintf(myhostname, sizeof(myhostname), "ws2812-%08x", ESP.getChipId());
  wifi_station_set_hostname(myhostname);

  EEPROM.begin(512);
  EEPROM.get(0, eepromData);

  if (digitalRead(buttonPin) == LOW) {
    delay(500);
    if (digitalRead(buttonPin) == LOW) {
      Serial.println("Button pressed, going into configuration mode");
      configuration_mode();
    }
  } else if (eepromData.configured != 1) {
    Serial.println("EEPROM is empty, going into configuration mode");
    configuration_mode();
  } else {
    Serial.println("Entering run mode");
    WiFi.mode(WIFI_STA);
    WiFi.begin(eepromData.ssid, eepromData.passphrase);
    pixels.updateType(eepromData.colourorder + NEO_KHZ800);
    pixels.updateLength(eepromData.pixelcount);
    pixels.setPin(dataPin);
    pixels.begin();
    for (int i = 0; i < eepromData.pixelcount; i++) {
      pixels.setPixelColor(i, pixels.Color(1, 1, 1));
    }
    pixels.show();
    Udp.begin(udpPort);
  }
}

void loop() {
  static boolean waitingForWiFi = true;

  if (waitingForWiFi) {
    if (WiFi.status() == WL_CONNECTED) {
      ip = WiFi.localIP();
      Serial.print("WiFi ready: SSID=");
      Serial.print(eepromData.ssid);
      Serial.print(" IP=");
      Serial.print(ip);
      Serial.print(" HOSTNAME=");
      Serial.println(myhostname);
      waitingForWiFi = false;
    }
  }

  udpLoop();
  ledLoop();

  uint8_t newButtonState = digitalRead(buttonPin);
  if ((newButtonState == LOW) && (buttonState == HIGH)) {
    ++ledMode;
    ledModeChanged = true;
    Serial.print("Button pressed, mode=");
    Serial.println(ledMode, DEC);
    buttonState = LOW;
  } else if ((newButtonState == HIGH) && (buttonState == LOW)) {
    buttonState = HIGH;
  }

  // give time to the ESP8266 WiFi stack
  yield();
}

void udpLoop() {
  unsigned int packetSize;
  IPAddress remoteIp;
  int remotePort;
  int len = 0;

  packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    remoteIp = Udp.remoteIP();
    remotePort = Udp.remotePort();

#ifdef DEBUG_PACKETS
    Serial.print("Packet received size=");
    Serial.print(packetSize);
    Serial.print(" from=");
    Serial.print(remoteIp);
    Serial.print(":");
    Serial.print(remotePort);
#endif

    len = Udp.read(inboundMessage, sizeof(inboundMessage));

#ifdef DEBUG_PACKETS
    if (len > 0) {
      Serial.print(" data=");
      for (int i = 0; i < len; i++) {
        if (inboundMessage[i] < 16) {
          Serial.print('0');
        }
        Serial.print(inboundMessage[i], HEX);
        Serial.print(' ');
      }
    }
    Serial.println();
#endif
    udpMessageHandler(len,remoteIp,remotePort);
  }
}

void ledLoop() {

  switch (ledMode) {
  case 0:
    // black
    singleColour(0, 0, 0);
    break;
  case 1:
    // red
    singleColour(255, 0, 0);
    break;
  case 2:
    // dim red
    singleColour(63, 0, 0);
    break;
  case 3:
    // green
    singleColour(0, 255, 0);
    break;
  case 4:
    // yellow
    singleColour(255, 255, 0);
    break;
  case 5:
    // blue
    singleColour(0, 0, 255);
    break;
  case 6:
    // magenta
    singleColour(255, 0, 255);
    break;
  case 7:
    // cyan
    singleColour(0, 255, 255);
    break;
  case 8:
    // white
    singleColour(255, 255, 255);
    break;
  case 9:
    hsvScroll();
    break;
  case 10:
    hsvFade();
    break;
  case 11:
    christmasRedAndGreen();
    break;
  case 12:
    twinkle();
    break;
  case 13:
    redNightLight();
    break;
  case 14:
    flames();
    break;
  case 255:
    // network mode - no action
    break;
  default:
    // default to black
    singleColour(0, 0, 0);
    ledMode = 0;
  }
}

void singleColour(uint8_t red, uint8_t green, uint8_t blue) {

  for (int i = 0; i < eepromData.pixelcount; i++) {
    pixels.setPixelColor(i, pixels.Color(red * eepromData.scalered / 255,
                                         green * eepromData.scalegreen / 255,
                                         blue * eepromData.scaleblue / 255));
  }
  pixels.show();
}

void redNightLight() {
  static uint8_t level = 255;
  static unsigned long lastChange = 0;
  const uint8_t minimum = 63;
  const unsigned long runtime = 1800000;
  unsigned long interval = runtime / (255 - minimum);

  if (ledModeChanged) {
    level = 255;
    singleColour(level, 0, 0);
    ledModeChanged = false;
    lastChange = millis();
  } else {
    if (millis() - lastChange > interval) {
      if (level > minimum) {
        level = level - 1;
      }
      singleColour(level, 0, 0);
      lastChange = millis();
    }
  }
}

void hsvFade() {
  static int hue = 0;
  static unsigned long lastChange = 0;
  unsigned long interval = 50;

  if (millis() - lastChange > interval) {
    for (int i = 0; i < eepromData.pixelcount; i++) {
      pixels.setPixelColor(i, ledHSV(hue, 1.0, 1.0));
    }
    pixels.show();
    if (hue >= 359) {
      hue = 0;
    } else {
      ++hue;
    }
    lastChange = millis();
  }
}

void hsvStatic() {

  for (int i = 0; i < eepromData.pixelcount; i++) {
    pixels.setPixelColor(i, ledHSV(i * 360 / eepromData.pixelcount, 1.0, 1.0));
  }
  pixels.show();
}

void hsvScroll() {
  static int hue = 0;
  static unsigned long lastChange = 0;
  unsigned long interval = 50;

  if (millis() - lastChange > interval) {
    for (int i = 0; i < eepromData.pixelcount; i++) {
      pixels.setPixelColor(i, ledHSV(((i * 360 / eepromData.pixelcount) + hue) % 360, 1.0, 1.0));
    }
    pixels.show();
    if (hue > 359) {
      hue = 0;
    } else {
      ++hue;
    }
    lastChange = millis();
  }
}

// Flames based on FastLED example code Fire2012 by Mark Kriegsman, July 2012 from
// https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
void flames() {
  // COOLING: How much does the air cool as it rises?
  // Less cooling = taller flames.  More cooling = shorter flames.
  // Default 50, suggested range 20-100 
  #define COOLING  55

  // SPARKING: What chance (out of 255) is there that a new spark will be lit?
  // Higher chance = more roaring fire.  Lower chance = more flickery fire.
  // Default 120, suggested range 50-200.
  #define SPARKING 120
  
  static byte heat[255];
  static unsigned long lastChange = 0;
  unsigned long interval = 16;
  int randomnum;
  

  if (millis() - lastChange > interval) {
    // Step 1.  Cool down every cell a little
    for (int i = 0; i < eepromData.pixelcount; i++) {
       randomnum = heat[i] - random(0, ((COOLING * 10) / eepromData.pixelcount) + 2);
       heat[i] = max(randomnum, 0 );
    }
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= eepromData.pixelcount - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random(255) < SPARKING ) {
      int y = random(7);
      heat[y] = min((heat[y] + random(160,255)), 0xFF);
    }
    for( int j = 0; j < eepromData.pixelcount; j++) {
      pixels.setPixelColor(j, ledTemp(heat[j]));
    }
    pixels.show();
    lastChange = millis();
  }
}

void christmasRedAndGreen() {
  static unsigned long lastChange = 0;
  unsigned long interval = 200;
  const uint32_t red = pixels.Color(255, 0, 0);
  const uint32_t green = pixels.Color(0, 255, 0);

  if (ledModeChanged) {
    // randomise all of the pixels
    for (int i = 0; i < eepromData.pixelcount; i++) {
      if (random(0, 2) == 0) {
        pixels.setPixelColor(i, red);
      } else {
        pixels.setPixelColor(i, green);
      }
    }
    pixels.show();
    ledModeChanged = false;
  }

  if (millis() - lastChange > interval) {
    int i = random(0, eepromData.pixelcount);
    if (pixels.getPixelColor(i) == green) {
      pixels.setPixelColor(i, red);
    } else {
      pixels.setPixelColor(i, green);
    }
    pixels.show();
    lastChange = millis();
  }
}

void twinkle() {
  static unsigned long lastChange = 0;
  static unsigned long lastPulse = 0;
  static uint8_t levels[MAX_PIXELS];
  static uint8_t current[MAX_PIXELS];
  unsigned long pulseInterval = 250;
  unsigned long changeInterval = 10;
  const uint8_t low = 50;
  const uint8_t high = 255;
  const uint8_t step = 10;

  if (ledModeChanged) {
    for (int i = 0; i < eepromData.pixelcount; i++) {
      levels[i] = low;
      current[i] = 0;
    }
    ledModeChanged = false;
  }

  if (millis() - lastPulse > pulseInterval) {
    int target = random(0, eepromData.pixelcount);
    if (current[target] == 0 && levels[target] == low) {
      current[target] = 1;
    }
    lastPulse = millis();
  }

  if (millis() - lastChange > changeInterval) {
    for (int i = 0; i < eepromData.pixelcount; i++) {
      if (current[i] == 1) {
        // this pixel is rising
        if (levels[i] + step > high) {
          // the pixel has reached maximum
          levels[i] = high;
          current[i] = 0;
        } else {
          levels[i] = levels[i] + step;
        }
      } else if (levels[i] > low) {
        // this pixel is falling
        if (levels[i] - step < low) {
          levels[i] = low;
        } else {
          levels[i] = levels[i] - step;
        }
      }
      pixels.setPixelColor(i, pixels.Color(levels[i], levels[i], levels[i]));
    }
    pixels.show();
    lastChange = millis();
  }
}

void udpMessageHandler(int len, IPAddress fromIP, int fromPort) {

  switch (inboundMessage[0]) {
  case 0x01: {
    // static colour: r, g, b
    singleColour(inboundMessage[1], inboundMessage[2], inboundMessage[3]);
    ledMode = 255;
    break;
  }
  case 0x02:
    // preset mode: n
    ledMode = inboundMessage[1];
    ledModeChanged = true;
    break;
  case 0x03: {
    // full sequence: r, g, b, r, g, b, ...
    uint8_t red, green, blue;
    uint16_t pos = 1;
    uint16_t pixel = 0;
    while ((pos + 2) < len) {
      red = inboundMessage[pos];
      green = inboundMessage[pos + 1];
      blue = inboundMessage[pos + 2];
      pixels.setPixelColor(pixel, pixels.Color(red, green, blue));
      pixel++;
      pos += 3;
    }
    pixels.show();
    ledMode = 255;
    break;
  }
case 0xFF: {
    // respond to a message with device details
    Udp.beginPacket(fromIP, fromPort);
    for (byte i = 0; i < strlen(myhostname); ++i)
      Udp.write(myhostname[i]);
    
    if (Udp.endPacket())
      Serial.println("endPacket successful");
    else {
      Serial.println("endPacket failed");
    }
    
    break;
  }
 }
}
// Convert a given HSV (Hue Saturation Value) to RGB(Red Green Blue) and set
// the led to the color
//  h is hue value, integer between 0 and 360
//  s is saturation value, double between 0 and 1
//  v is value, double between 0 and 1
// http://splinter.com.au/blog/?p=29
uint32_t ledHSV(int h, double s, double v) {
  double r = 0;
  double g = 0;
  double b = 0;

  double hf = h / 60.0;

  int i = floor(h / 60.0);
  double f = h / 60.0 - i;
  double pv = v * (1 - s);
  double qv = v * (1 - s * f);
  double tv = v * (1 - s * (1 - f));

  switch (i) {
  case 0:
    r = v;
    g = tv;
    b = pv;
    break;
  case 1:
    r = qv;
    g = v;
    b = pv;
    break;
  case 2:
    r = pv;
    g = v;
    b = tv;
    break;
  case 3:
    r = pv;
    g = qv;
    b = v;
    break;
  case 4:
    r = tv;
    g = pv;
    b = v;
    break;
  case 5:
    r = v;
    g = pv;
    b = qv;
    break;
  }

  // set each component to a integer value between 0 and 255
  int red = constrain((int)255 * r, 0, 255);
  int green = constrain((int)255 * g, 0, 255);
  int blue = constrain((int)255 * b, 0, 255);

  return pixels.Color(red * eepromData.scalered / 255,
                      green * eepromData.scalegreen / 255,
                      blue * eepromData.scaleblue / 255);
}

uint32_t ledTemp(int temperature) {
  double r = 0;
  double g = 0;
  double b = 0;

    // Scale 'heat' down from 0-255 to 0-191,
    // which can then be easily divided into three
    // equal 'thirds' of 64 units each.
    uint8_t t192 = map(temperature, 0, 255,0, 192);

    // calculate a value that ramps up from
    // zero to 255 in each 'third' of the scale.
    uint8_t heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2; // scale up to 0..252

    // now figure out which third of the spectrum we're in:
    if( t192 & 0x80) {
        // we're in the hottest third
        r = 255; // full red
        g = 255; // full green
        b = heatramp; // ramp up blue

    } else if( t192 & 0x40 ) {
        // we're in the middle third
        r = 255; // full red
        g = heatramp; // ramp up green
        b = 0; // no blue

    } else {
        // we're in the coolest third
        r = heatramp; // ramp up red
        g = 0; // no green
        b = 0; // no blue
    }

  // set each component to a integer value between 0 and 255
  int red = constrain((int)255 * r, 0, 255);
  int green = constrain((int)255 * g, 0, 255);
  int blue = constrain((int)255 * b, 0, 255);

  return pixels.Color(red * eepromData.scalered / 255,
                      green * eepromData.scalegreen / 255,
                      blue * eepromData.scaleblue / 255);
}

void configRootHandler() {
  const char form[] =
      "<form method=\"POST\" action=\"update\">SSID: <input type=\"text\" "
      "name=\"ssid\" /><br/>"
      "Passphrase: <input type=\"text\" name=\"passphrase\" /><br/>"
      "Pixel Count: <input type=\"text\" name=\"pixelcount\" /><br/>"
      "Colour Order: <select name=\"colourorder\">"
      "<option value=\"6\">RGB</option>"
      "<option value=\"9\">RBG</option>"
      "<option value=\"82\">GRB</option>"
      "<option value=\"161\">GBR</option>"
      "<option value=\"88\">BRG</option>"
      "<option value=\"164\">BGR</option>"
      "</select><br/>"
      "Scaling: "
      "R<input type=\"text\" name=\"scalered\" value=\"255\"/>"
      "G<input type=\"text\" name=\"scalegreen\" value=\"255\"/>"
      "B<input type=\"text\" name=\"scaleblue\" value=\"255\"/><br/>"
      "<input type=\"submit\" /></form>";

  server.send(200, "text/html", form);
}

void configUpdateHandler() {

  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "ssid") {
      server.arg(i).toCharArray(eepromData.ssid, sizeof(eepromData.ssid));
    }
    if (server.argName(i) == "passphrase") {
      server.arg(i).toCharArray(eepromData.passphrase, sizeof(eepromData.passphrase));
    }
    if (server.argName(i) == "pixelcount") {
      eepromData.pixelcount = server.arg(i).toInt();
    }
    if (server.argName(i) == "colourorder") {
      eepromData.colourorder = server.arg(i).toInt();
    }
    if (server.argName(i) == "scalered") {
      eepromData.scalered = server.arg(i).toInt();
    }
    if (server.argName(i) == "scalegreen") {
      eepromData.scalegreen = server.arg(i).toInt();
    }
    if (server.argName(i) == "scaleblue") {
      eepromData.scaleblue = server.arg(i).toInt();
    }
    eepromData.configured = 1;
    EEPROM.put(0, eepromData);
    EEPROM.commit();
  }
  server.send(200, "text/html", "<p>Settings updated</p>");
}

void configuration_mode() {

  // set first three pixels to dim white to acknowledge configuration mode
  pixels.updateType(NEO_RGB + NEO_KHZ800);
  pixels.updateLength(3);
  pixels.setPin(dataPin);
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(63, 63, 63));
  pixels.setPixelColor(1, pixels.Color(63, 63, 63));
  pixels.setPixelColor(2, pixels.Color(63, 63, 63));
  pixels.show();

  // go into access point mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(myhostname);
  delay(100);
  ip = WiFi.softAPIP();

  // display access details
  Serial.print("WiFi AP: SSID=");
  Serial.print(myhostname);
  Serial.print(" URL=http://");
  Serial.print(ip);
  Serial.println("/");

  // set first three pixels to Red-Green-Blue to indicate that configuration
  // mode AP is ready, and to help the user identify the correct colour order
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.setPixelColor(1, pixels.Color(0, 255, 0));
  pixels.setPixelColor(2, pixels.Color(0, 0, 255));
  pixels.show();

  server.on("/", configRootHandler);
  server.on("/update", configUpdateHandler);
  server.begin();

  while (1) {
    server.handleClient();
  }
}
