#ifndef CLIENT_H
#define CLIENT_H
/*
   This program login into ESP8266_locsolo_server on ubuntu. Gets the A0 pin status from the server then set it. Also send Vcc voltage and temperature, etc.
   When A0 is HIGH: ESP8266 loggin happens every 30seconds, if it is LOW ---> deep sleep for x seconds
   by Norbi

   3V alatt ne nyisson ki a szelep, de ha nyitva van akkor legyen egy deltaU feszĂĽltsĂ©g ami alatt csukodk be (pl 2.9V)
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include <DHT.h>
#include <Ticker.h>
#include <DNSServer.h>
#include "BMP280.h"
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>

//----------------------------------------------------------------settings---------------------------------------------------------------------------------------------------------------------------------------------//
#define WIFI_CONNECTION_TIMEOUT           15                              //Time for connecting to wifi in seconds
#define WIFI_CONFIGURATION_PAGE_TIMEOUT   300                             //when cannot connect to saved wireless network, in seconds, this is the time until we can set new SSID in seconds
#define MAX_VALVE_SWITCHING_TIME_SECONDS  30                              //The time when valve is switched off in case of broken microswitch or mechanical failure in seconds
#define WEB_UPDATE_TIMEOUT_SECONDS        300                             //The time out for web update server in seconds 
#define SLEEP_TIME_NO_WIFI_SECONDS        3600                            //When cannot connect to wifi network, sleep time between two attempts
#define MINIMUM_DEEP_SLEEP_TIME_SECONDS   60                              //in seconds
#define VERSION                           "v1.0.0"
//---------------------------------------------------------------End of settings---------------------------------------------------------------------------------------------------------------------------------------//

//------------------------------------------------------------------------Do not edit------------------------------------------------------------------------------------------------
#define MINIMUM_DEEP_SLEEP_TIME           MINIMUM_DEEP_SLEEP_TIME_SECONDS * 1000000
#define SLEEP_TIME_NO_WIFI                SLEEP_TIME_NO_WIFI_SECONDS * 1000000    
#define SLEEP_TIME                        sleep_time_seconds * 1000000            //when watering is off, in microseconds
#define DELAY_TIME                        delay_time_seconds * 1000               //when watering is on, in miliseconds
#define WEB_UPDATE_TIMEOUT                WEB_UPDATE_TIMEOUT_SECONDS  * 1000      //time out for web update server
#define MAX_VALVE_SWITCHING_TIME          MAX_VALVE_SWITCHING_TIME_SECONDS*1000
#define DHT_PIN                           0
#define DHT_TYPE                          DHT22
#define VALVE_H_BRIDGE_RIGHT_PIN          12
#define VALVE_H_BRIDGE_LEFT_PIN           4
#define VALVE_SWITCH_ONE                  14
#define VALVE_SWITCH_TWO                  13
#define GPIO15                            15      //ennek kene lennie a voltage boost EN pinnek és az FSA3157 switch pinnek is
#define RXD_VCC_PIN                       3
#define SCL                               5
#define SDA                               2
#define FLOWMETER_PIN                     5
#define MQTT_SERVER                       "locsol.dynamic-dns.net"
//#define MQTT_SERVER                     "192.168.1.14"
#define FLOWMETER_CALIB_VOLUME            450.0
#define FLOWMETER_CALIB_VELOCITY          7.5
#define MINIMUM_VALVE_OPEN_VOLTAGE        3.0
#define SZELEP                            1
//--------------------------------------------------------------------End----------------------------------------------------------------------------------------------------------------------------------------------------//

void valve_turn_on();
void valve_turn_off();
int valve_state();
void valve_test();
void flow_meter_calculate_velocity();
void get_TempPressure();
void go_sleep(long long int microseconds);
void read_voltage();
void read_moisture();
void go_sleep_callback(WiFiManager *myWiFiManager);
void mqttsend_d(double payload, String device_id, String topic,char precision);
void http_update_answer(t_httpUpdate_return ret);
void mqttsend_i(int payload, char* device_id, char* topic);
void mqttsend_d(double payload, char* device_id, char* topic, char precision);

extern char device_id[25];

extern const char* mosquitto_user;
extern const char* mosquitto_pass;

#endif
