/**
   BasicHTTPClient.ino
    Created on: 24.05.2015
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

#include "secrets.h"


// Neutral 554
// Neg 360, 470
// Pos 744, 631
// Spacing ~80, ~110

enum class States {
  IDLE,
  EXIT,
  ENTER
};

constexpr int IDLE_THRESHOLD = 570;
constexpr int EXIT_THRESHOLD = 650;

// Max noise for the sensor when stationary
constexpr int MIN_MOVE = 20;

// Time with a trigger value to declare the door is stuck open
constexpr int STICK_TIME = 1000;
// Ignore triggers for this duration after an initial trigger. This is to avoid triggering on the swinging of the door
constexpr int SWING_TIME = 10000;

const char* ON_JSON = "{\"seg\":[{\"id\":2,\"bri\":255}]}";
const char* OFF_JSON = "{\"seg\":[{\"id\":2,\"bri\":0}]}";

long unsigned int next_print = 0;
long unsigned int last_move = 0;
long unsigned int event_start = 0;
int last_adc = 0;

States state = States::IDLE;

ESP8266WiFiMulti WiFiMulti;

void setup() {

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID, PASSWORD);
}

void loop() {
  int adcValue = analogRead(A0); /* Read the Analog Input value */
  
  unsigned long cur_time = millis();

  if (cur_time > next_print) {
    Serial.println(adcValue);
    next_print = cur_time + 500;
  }

  // Avoid repeat triggers if the door sticks mid swing
  bool stuck = false;
  if (adcValue > IDLE_THRESHOLD && abs(adcValue-last_adc) < MIN_MOVE) {
    if (cur_time - last_move > STICK_TIME) {
      stuck = true;
    }
  } else {
    last_move = cur_time;
  }

  // Only trigger an event if the door isn't stuck, and this is the first trigger in an event.
  if (!stuck && cur_time - event_start > SWING_TIME) {
    if (adcValue > EXIT_THRESHOLD && state != States::EXIT) {
      state = States::EXIT;
      Serial.print("Exit\n");
    } else if (adcValue > IDLE_THRESHOLD && state == States::IDLE) {
      state = States::ENTER;
      Serial.print("Enter?\n");
    }
  }  
  
  if (state != States::IDLE && adcValue < IDLE_THRESHOLD) {
    event_start = cur_time;
    state = States::IDLE;
    // wait for WiFi connection
    if (WiFiMulti.run() == WL_CONNECTED) {
      WiFiClient client;

      HTTPClient http;

      Serial.print("[HTTP] begin...\n");
      if (http.begin(client, "http://192.168.1.123/json/state")) {  // HTTP


        Serial.print("[HTTP] GET...\n");
        // start connection and send HTTP header

        const char* json = (state == States::EXIT) ? OFF_JSON : ON_JSON;
        int httpCode = http.POST((uint8_t*)json, strlen(json));

        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTP] GET... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();
            Serial.println(payload);
          }
        } else {
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
      } else {
        Serial.printf("[HTTP} Unable to connect\n");
      }
    }
  }
}