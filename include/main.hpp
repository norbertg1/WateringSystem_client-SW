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
#include "esp_certificates.h"
#include "communication.hpp"
#include "peripherals.hpp"
#include "filesystem.hpp"
#include "valve_control.hpp"
#include "update.hpp"

//----------------------------------------------------------------SETTINGS---------------------------------------------------------------------------------------------------------------------------------------------//
#define WIFI_CONNECTION_TIMEOUT           30                              //Time for connecting to wifi in seconds
#define WIFI_CONFIGURATION_PAGE_TIMEOUT   300                             //when cannot connect to saved wireless network, in seconds, this is the time until we can set new SSID in seconds
#define MAX_VALVE_SWITCHING_TIME_SECONDS  20                              //The time when valve is switched off in case of broken microswitch or mechanical failure in seconds
#define WEB_UPDATE_TIMEOUT_SECONDS        300                             //The time out for web update server in seconds 
#define SLEEP_TIME_NO_WIFI_SECONDS        30//3600                            //When cannot connect to wifi network, sleep time between two attempts
#define MINIMUM_DEEP_SLEEP_TIME_SECONDS   60                              //in seconds
//---------------------------------------------------------------End of settings---------------------------------------------------------------------------------------------------------------------------------------//

//------------------------------------------------------------------------CONSTANTS AND PIN SETTINGS------------------------------------------------------------------------------------------------
#define MINIMUM_DEEP_SLEEP_TIME           (long long) MINIMUM_DEEP_SLEEP_TIME_SECONDS * 1000000
#define SLEEP_TIME_NO_WIFI                (long long) SLEEP_TIME_NO_WIFI_SECONDS * 1000000    
#define SLEEP_TIME                        (long long) sleep_time_seconds * 1000000            //when watering is off, in microseconds
#define DELAY_TIME                        delay_time_seconds * 1000               //when watering is on, in miliseconds
#define WEB_UPDATE_TIMEOUT                WEB_UPDATE_TIMEOUT_SECONDS  * 1000      //time out for web update server
#define MAX_VALVE_SWITCHING_TIME          MAX_VALVE_SWITCHING_TIME_SECONDS*1000
#define VALVE_H_BRIDGE_RIGHT_PIN          26
#define VALVE_H_BRIDGE_LEFT_PIN           33
#define VALVE_SWITCH_ONE                  32
#define VALVE_SWITCH_TWO                  25
#define SCL                               22
#define SDA                               21
#define FLOWMETER_PIN                     14     //<---- eddig
#define EN_5V                             12    //5V enable or disable for flowmeter
//#define VOLTAGE_CALIB                     230
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

void go_sleep(float microseconds, int winter_state);
void go_sleep_callback(/*WiFiManager *myWiFiManager*/void *);
void print_out(String str);
void println_out(String str);
void alternative_startup();
void setup_pins();
void config_time();
String reset_reason(int icore);
void send_measurements_to_server();
void RTC_saveCRC();
void RTC_validateCRC();
void flowmeter_callback();
void mqtt_callback(char* topic, byte* payload, unsigned int length);

extern WiFiClientSecure WifiSecureClient;
extern mqtt mqtt_client;

extern char device_id[16];
extern const char* mosquitto_user;
extern const char* mosquitto_pass;
extern int sleep_time_seconds;
extern int delay_time_seconds; 
extern int remote_update, remote_log;
extern String ID;
extern int winter_state;
extern flowmeter Flowmeter;
extern voltage Battery;
extern valve_control Valve;

#endif