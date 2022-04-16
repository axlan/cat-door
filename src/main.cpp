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

States state = States::IDLE;

// #define EXT_ISR_PIN 2 ///< Interrupt pin connected to AS1115

ESP8266WiFiMulti WiFiMulti;

// volatile bool interrupted = false;
// void IRAM_ATTR interrupt() { interrupted = true; }

void setup() {

  
  // pinMode(EXT_ISR_PIN, INPUT);
  // attachInterrupt(digitalPinToInterrupt(EXT_ISR_PIN), interrupt, RISING);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID, PASSWORD);
}

const char* ON_JSON = "{\"seg\":[{\"id\":2,\"bri\":255}]}";
const char* OFF_JSON = "{\"seg\":[{\"id\":2,\"bri\":0}]}";

long unsigned int next_print = 0;

void loop() {
  int adcValue = analogRead(A0); /* Read the Analog Input value */

  if (millis() > next_print) {
    Serial.println(adcValue);
    next_print = millis() + 500;
  }

  if (adcValue > EXIT_THRESHOLD && state != States::EXIT) {
    state = States::EXIT;
    Serial.print("Exit\n");
  } else if (adcValue > IDLE_THRESHOLD && state == States::IDLE) {
    state = States::ENTER;
    Serial.print("Enter?\n");
  } else if (state != States::IDLE && adcValue < IDLE_THRESHOLD) {
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
    state = States::IDLE;
  }
}