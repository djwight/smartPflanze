#include <PubSubClient.h>

#include <MQTT.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>

#include <Arduino.h>
#include <U8g2lib.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>

#include "secrets.h"
#include "constants.h"

//// OLED screen driver
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// BME280 driver
Adafruit_BME280 bme;

// configure MQTT for pub/sub
#define AWS_IOT_PUBLISH_TOPIC "iot_home/bitter_orange"
WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);


//////////////////////////////////////////////////////////// 
///                ACCESSORY FUNCTIONS
////////////////////////////////////////////////////////////

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
 * Initialises connection to the wifi
 * 
 * @param ssid SSID of the wifi
 * @param password wifi passord
 */
void setupWifi(const char* ssid, const char* password) {
  u8g2.drawStr(0, 10, "Connecting to:");
  u8g2.drawStr(0, 20, ssid);
  u8g2.sendBuffer();
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  // Print connecting until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    u8g2.setCursor(0,40);
    u8g2.print("WiFi Connecting...");
    sendClear(500, "display");
  }
}

void connectIot() {
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);
  while (!client.connect(THINGNAME)) {
    delay(200);
    u8g2.setCursor(0,40);
    u8g2.print("IoT Core Connecting...");
    sendClear(500, "display");
  }
  u8g2.setCursor(0,40);
  u8g2.print("AWS IoT Core Connected!");
  sendClear(1000, "display");
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

void publishMessage(sensVals reading) {
  StaticJsonDocument<200> doc;
  doc["id"] = "bitter_orange";
  doc["soil_moist"] = reading.soil;
  doc["temp"] = reading.temp;
  doc["humidity"] = reading.humid;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
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
  //WiFi set-up
  setupWifi(WIFI_SSID, WIFI_PASS);
}


void loop() {
  // Read sensor data into struct
  sensVals reading;
  reading = readSensors();
  oledRow(0, 10, "SOIL moist:   ", reading.soil, " %");
  oledRow(0, 50, "Temperature:   ", reading.temp, " *C");
  oledRow(0, 62, "Humidity   ", reading.humid, " %");
  if (reading.soil < 20) {
    u8g2.drawStr(0, 26, "!! NEEDS WATER !!");
  }
  sendClear(1000, "buffer");
}
