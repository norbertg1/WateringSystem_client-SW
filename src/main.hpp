#ifndef MAIN_H
#define MAIN_H
/*
   This program login into ESP8266_locsolo_server on ubuntu. Gets the A0 pin status from the server then set it. Also send Vcc voltage and temperature, etc.
   When A0 is HIGH: ESP8266 loggin happens every 30seconds, if it is LOW ---> deep sleep for x seconds
   by Norbi

   3V alatt ne nyisson ki a szelep, de ha nyitva van akkor legyen egy deltaU feszĂĽltsĂ©g ami alatt csukodk be (pl 2.9V)
*/
#define ESP32
#include <Arduino.h>
#include "FS.h"
#include <PubSubClient.h>
//#include <ESP8266httpUpdate.h>
#include "HTTPUpdate.h"

//----------------------------------------------------------------settings---------------------------------------------------------------------------------------------------------------------------------------------//
#define WIFI_CONNECTION_TIMEOUT           30                              //Time for connecting to wifi in seconds
#define WIFI_CONFIGURATION_PAGE_TIMEOUT   300                             //when cannot connect to saved wireless network, in seconds, this is the time until we can set new SSID in seconds
#define MAX_VALVE_SWITCHING_TIME_SECONDS  20                              //The time when valve is switched off in case of broken microswitch or mechanical failure in seconds
#define WEB_UPDATE_TIMEOUT_SECONDS        300                             //The time out for web update server in seconds 
#define SLEEP_TIME_NO_WIFI_SECONDS        30//3600                            //When cannot connect to wifi network, sleep time between two attempts
#define MINIMUM_DEEP_SLEEP_TIME_SECONDS   60                              //in seconds
//---------------------------------------------------------------End of settings---------------------------------------------------------------------------------------------------------------------------------------//

//------------------------------------------------------------------------Do not edit------------------------------------------------------------------------------------------------
#define MINIMUM_DEEP_SLEEP_TIME           (long long) MINIMUM_DEEP_SLEEP_TIME_SECONDS * 1000000
#define SLEEP_TIME_NO_WIFI                (long long) SLEEP_TIME_NO_WIFI_SECONDS * 1000000    
#define SLEEP_TIME                        (long long) sleep_time_seconds * 1000000            //when watering is off, in microseconds
#define DELAY_TIME                        delay_time_seconds * 1000               //when watering is on, in miliseconds
#define WEB_UPDATE_TIMEOUT                WEB_UPDATE_TIMEOUT_SECONDS  * 1000      //time out for web update server
#define MAX_VALVE_SWITCHING_TIME          MAX_VALVE_SWITCHING_TIME_SECONDS*1000
#define DHT_TYPE                          DHT22
/*#define DHT_PIN                           0     //<---- innet kikommentezni oldhoz
#define VALVE_H_BRIDGE_RIGHT_PIN          12
#define VALVE_H_BRIDGE_LEFT_PIN           4
#define VALVE_SWITCH_ONE                  14
#define VALVE_SWITCH_TWO                  13
#define GPIO15                            15      //ennek kene lennie a voltage boost EN pinnek és az FSA3157 switch pinnek is
#define SCL                               5
#define SDA                               2
#define FLOWMETER_PIN                     5  */   //<---- eddig
#define RXD_VCC_PIN                       3
#define TXD_PIN                           1
#define VOLTAGE_CALIB                     230
#define MQTT_SERVER                       "locsol.dynamic-dns.net"
#define FTP_SERVER                        "192.168.1.101"
#define FLOWMETER_CALIB_VOLUME            450.0 //Pulses per Liter: 450
#define FLOWMETER_CALIB_VELOCITY          7.5   //Pulse frequency (Hz) / 7.5 = flow rate in L/min
#define MINIMUM_UPDATE_VOLTAGE            3.1   //If valve is closed
#define MINIMUM_VALVE_OPEN_VOLTAGE        3.1   //If valve is closed
#define VALVE_CLOSE_VOLTAGE               3.05   //If valve is open
#define MAX_LOG_FILE_SIZE                 409600
#define SZELEP                            1
#define FILE_SYSTEM                       0
#define SERIAL_PORT                       1
#define CONFIG_TIME                       1
//--------------------------------------------------------------------End----------------------------------------------------------------------------------------------------------------------------------------------------//

//--------------------------------------------------------------------old---------------------------------------------------------------------------------------------------------------------------------------------------//
#define VALVE_H_BRIDGE_RIGHT_PIN          12
#define VALVE_H_BRIDGE_LEFT_PIN           15
#define VALVE_SWITCH_ONE                  5
#define VALVE_SWITCH_TWO                  13
#define GPIO15                            0
#define FLOWMETER_PIN                     4
#define SCL                               3
#define SDA                               14
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//


void valve_turn_on();
void valve_turn_off();
int  valve_state();
void valve_test();
void flow_meter_calculate_velocity();
void get_TempPressure();
void go_sleep(float microseconds, int winter_state);
float read_voltage();
void read_moisture();
void go_sleep_callback(/*WiFiManager *myWiFiManager*/void *);
void mqttsend_d(double payload, String device_id, String topic,char precision);
void http_update_answer(t_httpUpdate_return ret);
void mqttsend_i(int payload, char* device_id, char* topic);
void mqttsend_d(double payload, char* device_id, char* topic, char precision);
File create_file();
void close_file();
void print_out(String str);
void println_out(String str);
void alternative_startup();
void format();
void setup_pins();
void config_time();
void flow_meter_interrupt();
void RTC_save();
void format_now();
void valve_open_on_button();
void winter_mode();
void valve_open_on_switch();
int get_reset_reason(int icore);
String reset_reason(int icore);

//extern WiFiClientSecure espClient;
extern WiFiClient espClient;
extern PubSubClient client;
//extern ESP8266WebServer server;

extern char device_id[16];
extern const char* mosquitto_user;
extern const char* mosquitto_pass;
extern int on_off_command;
extern int mqtt_done;
extern int sleep_time_seconds;
extern int delay_time_seconds; 
extern int remote_update, remote_log;
extern String ID;
extern int winter_state;

#endif