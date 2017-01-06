#ifndef server_H_
#define server_H_

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <Time.h>
#include <TimeLib.h>
#include "Timer.h"
#include <DHT.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager

extern "C" {
#include "user_interface.h"
}
#include "HTMLpage.h"
#include "NTP_Time.h"
#include "client.h"
#include "server_response.h"

#define SLEEP_TIME_NO_WIFI_SECONDS  60                                    //when cannot connect to saved wireless network in seconds, this is the time until we can set new SSID
#define SLEEP_TIME_NO_WIFI          SLEEP_TIME_NO_WIFI_SECONDS * 1000000  //when cannot connect to saved wireless network, this is the time until we can set new wifi SSID
#define SLEEP_TIME_ACESS_POINT 3000                                      //Set timeout after acess point turn off
#define TIMER_PERIOD 60             //Ilyen gyakran mérek hőmérsékletet, légnyomást esetleg egyéb dolgot
#define DHT_AVARAGE 10              //Ennyi hőmérsékeltmérésből mérek átlagot
#define TIME_SYNC_INTERVAL 86400
#define TIME_ZONE 1
#define DAYLIGHT_SAVING 1
#define SECS_PER_HOUR 3600
#define DHT_POWER 12
#define DHT_PIN 5
#define DHT_TYPE DHT22
#define DHT_SENSOR_MEMORY 100     //about at 0 Celsius the readout of DHT22 sensor is fluctuating, the atmenet at 0 Celsius is not continous, with this variable I investigating the problem
#define AUTO_WATERING_HOUR    7   //Az automatikus öntözés kezdőpontja - default
#define AUTO_WATERING_MINUTE  0
#define AUTO_WATERING_TIME  100   //Az automatikus öntözés hossza másodpercekben
#define LOGIN_TIME_OUT 1000         //Azaz ido amikor a locsolo kliensre ami csatlakozott time outot ír ki, mert nem csatlakozik
#define LOCSOLO_NUMBER  2         //A locslók száma
#define LOCSOL_MEMORY 800
#define SENSOR_MEMORY 800
#define ENABLE_IP 0                 //Bekapcsolja a legutóbbi 10 csatlakozott eszköz IP címének megjegyzését és kijelzését
#define IP_TIMEOUT  3600            //ha ugyanarrola az IPről jelentkezek be ennyi másodpercnek kell eltelnie hogy megjegyezze
#define OTA_ARDUINO_IDE 1           //OTA via Arduino
#define OTA_WEB_BROWSER 1           //OTA via Web Browser
#define ENABLE_FLASH 1
#define TELNET_DEBUG 0
//-------Flash memory map
#define mem_sector0             0x7b000
#define mem_sector1             0x7c000
#define mem_sector2             0x7d000
#define mem_sector3             0x7e000

typedef struct
{
  uint16_t Year;
  short   Month;
  short   Day;
  short   Weekday;
  short   Hour;
  short   Minute;
  short   Second;
}Time;
Time server_start;

struct __attribute__((aligned(4))) Locsolo
{
  char      Name[9];
  char      alias[20];              //hőmérséklet neve
  char      short_name;             //rovid locsolo neve takarekossag vegett
  short     set=LOW;                //Állapot amire állítom
  short     state=LOW;              //Állapot amiben jelenleg van
  short     autom=LOW;              //Automata öntözés be-ki kapcsolása
  short     timeout=LOW;            //Ha nincs kapcsolat a kliensel
  uint16_t  voltage[LOCSOL_MEMORY]; //A locsoló akumulátor feszültsége
  int16_t   temp[LOCSOL_MEMORY];    //A locsolo alltal mert feszultseg
  int8_t    humidity;               //A locsoló álltal mért páratartalom
  uint16_t  count=0;                //hányszor mértem meg a feszültséget
  time_t    login_epoch=0;          //A kliensel létesített legutóbbi kapcsolat időpontja
  time_t    watering_epoch=0;       //Az automata öntözés időpontja epoch-ban
  time_t    duration=600;           //Eddig tart az ontozes
  Time      auto_watering_time;     //Az automata öntözés időpontja óra, perc, másodperc
  uint8_t   temperature_graph;      //Toggle on/off temperature graph
  uint8_t   voltage_graph;          //Toggle on/off humidity graph
  int16_t   temp_max=-273;          //napi minimum hőmerseklet
  int16_t   temp_min=999;           //napi maximum hőmérséklet
  uint8_t   auto_flag=0;            //for automated watering
};
struct Locsolo locsolo[LOCSOLO_NUMBER];

typedef struct
{
  float           temperature_avg=0;          //A jelenlegi hőmérséklet, a measuerd értéke átlaga
  float           temperature_measured;
  float           avg_3h;         //11,13,15 órás átalghőmérséklet
  float           avg_3h_temp;    //11,13,15 órás átlaghőmérséklethez kell
  uint8_t         avg_nr;         //11,13,15 órás átlaghőmérséklethez kell
  float           avg_previous=0;    //a mar kiszamitott előzőnapi átlag hőmérsékelt
  float           avg_now=0;        //a mai napi hőmérséklet amiből az átlag lesz számítva, akkumulálódik állandóan
  uint16_t        daily_avg_cnt=0;       //ennyi darab értékből számolom ki a napi átlaghőmérsékletet
  float           Min=999;       //minimum hőmérséklet
  float           Max=-273.15;   //maximum hőmérséklet
  float           heat_index;   //hőérzet
  float           humidity_avg=0;     //
  float           humidity_measured;
  float           humidity_min=100;  
  float           humidity_max=0;  
  uint8_t         water_points=0;
  uint8_t         water_points_increase;
  int16_t         temperature_saved[SENSOR_MEMORY];
  uint8_t         humidity_saved[SENSOR_MEMORY];
  unsigned long   epoch_saved;                        //Ezt majd kidobni
  unsigned long   epoch_now;
  uint16_t        epoch_saved_dt[SENSOR_MEMORY];
  unsigned long   time_previous;
  uint16_t        count=0;
  uint8_t         count_dht=0;
  uint8_t         temperature_graph=1; //Toggle on/off temperature graph
  uint8_t         humidity_graph=1;    //Toggle on/off humidity graph
  uint8_t         thisday=0;            //for once/day running codes
  uint8_t         wifi_reset;         //Number of reset becouse of bad wifi signal
  unsigned long lastlogin_epoch;
  uint16_t login[SENSOR_MEMORY];
}Sensor;
Sensor __attribute__((aligned(4))) sensor;

struct  Adress
{
  String          IP;              //connected clients IP adress
  unsigned long   epoch;     //the time when the client is connected
  uint8_t         count=0;
};
Adress adress[10];

bool      flag = 0;
time_t    epoch_previous=0;           //Mielott aktualizallom az idot, elmentem a jelenlegit arra az esetre ha nem mukodne az aktualizacio
uint16_t  timeout=0;
uint32_t  temp_expire;
uint8_t   wifinc_count=0;
float     dht_temp[DHT_SENSOR_MEMORY];                    //about at 0 Celsius the readout of DHT22 sensor is fluctuating
float     dht_temp_avg[DHT_SENSOR_MEMORY/DHT_AVARAGE];    //the atmenet at 0 Celsius is not continous, with this variable I investigating the problem
bool      timer_flag;
DHT dht(DHT_PIN,DHT_TYPE);
Timer timer1;

const char* host = "watering";
const char* ssid = "wifi";
const char* password = "";
 
//WiFiServer server(80);
ESP8266WebServer server ( 80 );
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP udp;  // A UDP instance to let us send and receive packets over UDP

uint16_t dT;

void periodic_timer(); 
void DHT_sensor_read(struct Locsolo *locsol,uint8_t number);
void auto_ontozes(struct Locsolo *locsol,uint8_t number);
void load_default(struct Locsolo *locsol,uint8_t number);
void reset_cmd();
void Telnet_print(uint8_t n,...);
void read_flash(Sensor *sens, struct Locsolo *locsol);
void write_flash(Sensor *sens, struct Locsolo *locsol);
void ota_arduino_ide();
void loggin_time();
inline void ota_web_browser();
inline void telnet_debug();
void client_logintrack();

#if TELNET_DEBUG
  WiFiServer  TelnetDebug(18266);
  WiFiClient  TelnetDebugClient;
#endif

#if OTA_WEB_BROWSER
  ESP8266HTTPUpdateServer httpUpdater;
#endif

#endif
