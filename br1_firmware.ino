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
uint8_t ledMode = 10;
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
    ledMode++;
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
    udpMessageHandler(len);
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
    // green
    singleColour(0, 255, 0);
    break;
  case 3:
    // yellow
    singleColour(255, 255, 0);
    break;
  case 4:
    // blue
    singleColour(0, 0, 255);
    break;
  case 5:
    // magenta
    singleColour(255, 0, 255);
    break;
  case 6:
    // cyan
    singleColour(0, 255, 255);
    break;
  case 7:
    // white
    singleColour(255, 255, 255);
    break;
  case 8:
    // fading HSV
    hsvFade();
    break;
  case 9:
    // static HSV
    hsvStatic();
    break;
  case 10:
    // scrolling HSV
    hsvScroll();
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

void udpMessageHandler(int len) {

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
  ip = WiFi.softAPIP();

  // display access details
  Serial.print("SSID ");
  Serial.println(myhostname);
  Serial.print("URL http://");
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
