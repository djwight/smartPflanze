#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <U8g2lib.h>
#include <Adafruit_BME280.h>


#include "secrets.h"
#include "constants.h"

// OLED screen driver
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// BME280 driver
Adafruit_BME280 bme;

// Time client config for certificate validation
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Secure Wifi client config
WiFiClientSecure net;
BearSSL::X509List cert(AWS_CERT_CA);
BearSSL::X509List client_crt(AWS_CERT_CRT);
BearSSL::PrivateKey key(AWS_CERT_PRIVATE);

// ESP8266 MQTT client config
const int MQTT_PORT = 8883;
const char MQTT_PUB_TOPIC[] = "iot_home/bitter_orange";
MQTTClient client;


//////////////////////////////////////////////////////////// 
///                ACCESSORY FUNCTIONS
////////////////////////////////////////////////////////////

/**
 * Initialises connection to the wifi
 * 
 * @param ssid SSID of the wifi
 * @param password wifi passord
 */
void setupWifi(const char* ssid, const char* password) {
  u8g2.drawStr(0, 10, "Connecting to:");
  u8g2.drawStr(0, 20, WIFI_SSID);
  u8g2.sendBuffer();
  delay(1000);
  WiFi.hostname(THINGNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  // Print connecting until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    u8g2.setCursor(0,40);
    u8g2.print("WiFi Connecting...");
    sendClear(500, "display");
  }
  timeClient.begin();
  while(!timeClient.update()){
    timeClient.forceUpdate();
  }
  net.setX509Time(timeClient.getEpochTime());
}

/**
 * 
 */
void connectToMqtt() {
  u8g2.drawStr(0, 10, "MQTT Connecting...");
  u8g2.sendBuffer();
  while (!client.connected()) {
    if (client.connect(THINGNAME)) {
      u8g2.drawStr(0, 20, "MQTT Connected!");
      u8g2.sendBuffer();
    } else {
      u8g2.drawStr(0, 20, " MQTT Connection failed -> repeating in 5 seconds");
      u8g2.sendBuffer();
      delay(5000);
    }
  }
  delay(1000);
  u8g2.clearDisplay();
}


/**
 * Prints a row of text to the oled buffer.
 * 
 * @param x x-position for text display
 * @param y y_position for text display
 * @param text name of measurement
 * @param anInt value of measurement
 * @param ending unit of measurement
 */
void oledRow(int x, int y, char* text, int anInt, char* ending) {
  u8g2.setCursor(x,y);
  u8g2.print(text);
  u8g2.print(anInt);
  u8g2.print(ending);
}


/**
 * Sends the buffer to the display and clears for next measurements
 * 
 * @param ms_delay milliseconds to deplay before clearing
 * @param option whether to clear the "display" or "buffer"
 */
void sendClear(int ms_delay, char* option) {
  u8g2.sendBuffer();
  delay(ms_delay);
  if (option == "display"){
    u8g2.clearDisplay();
  } else {
    u8g2.clearBuffer();
  }
}


/**
 * Reads all the sensors and returns the data
 * 
 * @returns vals in a custom strut object
 */
sensVals readSensors() {
  sensVals vals;
  vals.temp = bme.readTemperature();
  vals.humid = bme.readHumidity();
  
  // Read analog and convert into relative humidity
  int analog_read = analogRead(A0);
  vals.soil = map(analog_read, WATER, AIR, 100, 0);
  return vals;
}

/**
 * 
 */
void publishMessage(sensVals reading) {
  StaticJsonDocument<200> doc;
  doc["id"] = "bitter_orange";
  doc["soil_moist"] = reading.soil;
  doc["temp"] = reading.temp;
  doc["humidity"] = reading.humid;
  doc["timestamp"] = timeClient.getEpochTime();
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(MQTT_PUB_TOPIC, jsonBuffer);
}

//////////////////////////////////////////////////////////// 
///           END OF ACCESSORY FUNCTIONS
////////////////////////////////////////////////////////////


void setup() {
  // OLED setup
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  // BME sensor set-up
  bme.begin(0x76);

  // Wifi set up
  setupWifi(WIFI_SSID, WIFI_PASS);

  // Set up secure MQTT connection
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
  client.begin(AWS_IOT_ENDPOINT, MQTT_PORT, net);
  connectToMqtt();
}

void loop() {
  // Read sensor data into struct
  sensVals reading;
  reading = readSensors();

  // Publish message to AWS IoT Core
  publishMessage(reading);

  // Send measurments to display
  u8g2.clearBuffer();
  oledRow(0, 10, "SOIL moist:   ", reading.soil, " %");
  oledRow(0, 50, "Temperature:   ", reading.temp, " *C");
  oledRow(0, 62, "Humidity   ", reading.humid, " %");
  if (reading.soil < 20) {
    u8g2.drawStr(0, 26, "!! NEEDS WATER !!");
  }
  u8g2.sendBuffer();
  delay(10000);
}
