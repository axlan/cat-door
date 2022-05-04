#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

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

// #define DEBUG_PRINT 1

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
bool should_send = false;

States state = States::IDLE;

const char* mqtt_server = "192.168.1.110";
const char* topic_sensor = "cat_door/sensor";
const char* topic_event = "cat_door/event";

ESP8266WiFiMulti WiFiMulti;

struct LogMessage
{
  uint32_t time;
  int32_t adc;
};

#define NUM_MESSAGES	(64)
LogMessage log_msgs[NUM_MESSAGES];
uint8_t num_msgs = 0;

void reconnect(PubSubClient& client) {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("catdoor")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID, PASSWORD);
}

void loop() {
  int adcValue = analogRead(A0); /* Read the Analog Input value */
  
  unsigned long cur_time = millis();


  // Avoid repeat triggers if the door sticks mid swing
  bool stuck = false;
  if (adcValue > IDLE_THRESHOLD && abs(adcValue-last_adc) < MIN_MOVE) {
    if (cur_time - last_move > STICK_TIME) {
      stuck = true;
    }
  } else {
    last_move = cur_time;
  }

#if DEBUG_PRINT
  if (cur_time > next_print) {
    Serial.printf("%d %d %d %d %lu %lu\n",adcValue, stuck, (int)state, should_send, cur_time, event_start);
    next_print = cur_time + 500;
  }
#endif

  if (abs(adcValue-last_adc) > MIN_MOVE) {
    if (num_msgs < NUM_MESSAGES) {
      Serial.printf("%d %d %d\n",adcValue,last_adc,num_msgs);
      log_msgs[num_msgs++] = {cur_time, adcValue};
    }
    last_adc = adcValue;
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
  
  if (!should_send && state != States::IDLE && adcValue < IDLE_THRESHOLD) {
    event_start = cur_time;
    should_send = true;
  }

  if (should_send && (cur_time - event_start > SWING_TIME)) {
    should_send = false;
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

      PubSubClient mqtt_client(client);
      mqtt_client.setServer(mqtt_server, 1883);
      if (!mqtt_client.connected()) {
        reconnect(mqtt_client);
      }
      if (mqtt_client.connected()) {
        mqtt_client.publish(topic_sensor, (uint8_t*)log_msgs, num_msgs * sizeof(LogMessage));
        const char * state_msg = (state == States::ENTER) ? "ENTER" : "EXIT";
        mqtt_client.publish(topic_event, state_msg, strlen(state_msg));
        mqtt_client.disconnect();
      }
      num_msgs = 0;
      WiFi.disconnect();
    } else {
      Serial.printf("Wifi Down\n");
    }
    state = States::IDLE;
  }
}
