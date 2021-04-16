#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "esp_certificates.h"
#include <HTTPUpdate.h>

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();
void setup_wifi();
void start_wifimanager();
void web_update_setup();
void web_update(long long minutes);
void send_log();
uint32_t calculateCRC32( const uint8_t *data, size_t length );
//String getSsidPass( String s );

void Wait_for_WiFi();

void http_update_answer(t_httpUpdate_return ret);

// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.
struct RTCData{
  uint32_t crc32;   // 4 bytes
  uint8_t channel;  // 1 byte,   5 in total
  uint8_t bssid[6]; // 6 bytes, 11 in total
  time_t  epoch;    // 4 bytes  15 in total
  uint8_t valid;    // 1 byte,  16 in total
  uint8_t attempts; // 1 byte,  17 in total
  uint8_t winter_state; // 1 byte,  18 in total
  uint8_t open_on_switch; // 1 byte,  19 in total ez arra kell, hogy a kapcsolóval is bel lehessen kapcsolni a locsolót
  char    ssid[32];       //32 byte, 51 in total
  char    psk[63];       //63 byte, 114 in total
  uint8_t padding[2];  // 2 byte,  116 in total
};
extern struct RTCData rtcData;


class mqtt : public PubSubClient{
   public:
    PubSubClient pubsubclient;
    mqtt(WiFiClientSecure& cclient);
    void callback_function(char* topic, byte* payload, unsigned int length);
    void reconnect();
    void send_d(double payload, char* device_id, char* topic, char precision);
    void send_i(int payload, char* device_id, char* topic);
    void send_s(const char *payload, char* device_id, char* topic);
};