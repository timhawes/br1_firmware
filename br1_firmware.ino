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
} eepromData;

char myhostname[64];
IPAddress ip;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel();
ESP8266WebServer server(80);
WiFiUDP Udp;
uint8_t inboundMessage[1500];
uint8_t ledMode = 0;
uint8_t buttonState = HIGH;

void setup() {

  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting... Hold button to enter configuration mode...");

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
    WiFi.begin(eepromData.ssid, eepromData.passphrase);
    Serial.print("Waiting for wifi...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
    }
    ip = WiFi.localIP();
    Serial.print(" SSID=");
    Serial.print(eepromData.ssid);
    Serial.print(" IP=");
    Serial.print(ip);
    Serial.print(" HOSTNAME=");
    Serial.println(myhostname);

    pixels.updateType(NEO_GRB + NEO_KHZ800);
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

  uint8_t newButtonState = digitalRead(buttonPin);
  if ((newButtonState == LOW) && (buttonState == HIGH)) {
    ledMode++;
    ledModeShow();
    buttonState = LOW;
  } else if ((newButtonState == HIGH) && (buttonState == LOW)) {
    buttonState = HIGH;
  }
}

void ledModeShow() {

  switch (ledMode) {
  case 0:
    singleColour(0, 0, 0);
    break;
  case 1:
    singleColour(255, 0, 0);
    break;
  case 2:
    singleColour(127, 0, 0);
    break;
  case 3:
    singleColour(7, 0, 0);
    break;
  case 4:
    singleColour(0, 255, 0);
    break;
  case 5:
    singleColour(0, 127, 0);
    break;
  case 6:
    singleColour(0, 7, 0);
    break;
  case 7:
    singleColour(0, 0, 255);
    break;
  case 8:
    singleColour(0, 0, 127);
    break;
  case 9:
    singleColour(0, 0, 7);
    break;
  case 10:
    singleColour(255, 255, 255);
    break;
  case 11:
    singleColour(127, 127, 127);
    break;
  case 12:
    singleColour(7, 7, 7);
    break;
  default:
    ledMode = 0;
    singleColour(0, 0, 0);
  }
}

void singleColour(uint8_t red, uint8_t green, uint8_t blue) {

  for (int i = 0; i < eepromData.pixelcount; i++) {
    pixels.setPixelColor(i, pixels.Color(red, green, blue));
  }
  pixels.show();
}

void udpMessageHandler(int len) {

  switch (inboundMessage[0]) {
  case 0x01: {
    // static colour: r, g, b
    singleColour(inboundMessage[1], inboundMessage[2], inboundMessage[3]);
    break;
  }
  case 0x02:
    // animated mode: n
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
    break;
  }
  }
}

void configRootHandler() {
  const char form[] =
      "<form method=\"POST\" action=\"update\">SSID: <input type=\"text\" "
      "name=\"ssid\" /><br/>"
      "Passphrase: <input type=\"text\" name=\"passphrase\" /><br/>"
      "Pixel Count: <input type=\"text\" name=\"pixelcount\" /><br/>"
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
