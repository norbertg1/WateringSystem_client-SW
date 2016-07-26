/*
 *Locsolórendszer és meteorológiai állomás. 
 *
 *
 *
 *
 *
 *
 *  Update on adress: locsolo.dynamic-dns.net:8266/update
 *
 *Created by Gál Norbert
 *  2015.8 - 2016.7
 * 
 */

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

#define LOGIN_TIME_OUT 1000         //Azaz ido amikor a locsolo kliensre ami csatlakozott time outot ír ki, mert nem csatlakozik
#define TIMER_PERIOD 60             //Ilyen gyakran mérek hőmérsékletet, légnyomást esetleg egyéb dolgot
#define DHT_AVARAGE 10
#define TIME_SYNC_INTERVAL 86400
#define TIME_ZONE 1
#define DAYLIGHT_SAVING 1
#define SECS_PER_HOUR 3600
#define DHT_PIN 5
#define DHT_TYPE DHT22
#define DHT_SENSOR_MEMORY 100     //about at 0 Celsius the readout of DHT22 sensor is fluctuating, the atmenet at 0 Celsius is not continous, with this variable I investigating the problem
#define AUTO_WATERING_HOUR    7   //Az automatikus öntözés kezdőpontja - default
#define AUTO_WATERING_MINUTE  0
#define AUTO_WATERING_TIME  100   //Az automatikus öntözés hossza másodpercekben
#define LOCSOLO_NUMBER  2         //A locslók száma
#define LOCSOL_MEMORY 800
#define SENSOR_MEMORY 800
#define ENABLE_IP 0                 //Bekapcsolja a legutóbbi 10 csatlakozott eszköz IP címének megjegyzését és kijelzését
#define IP_TIMEOUT  3600            //ha ugyanarrola az IPről jelentkezek be ennyi másodpercnek kell eltelnie hogy megjegyezze
#define OTA_ARDUINO_IDE 1           //OTA via Arduino
#define OTA_WEB_BROWSER 1           //OTA via Web Browser
#define ENABLE_FLASH 1
#define TELNET_DEBUG 1
//-------Flash memory map
#define mem_sector0             0x7b000
#define mem_sector1             0x7c000
#define mem_sector2             0x7d000
#define mem_sector3             0x7e000

void time_out(struct Locsolo *locsol, uint8_t number);
struct Locsolo printstatus1(struct Locsolo *locsol,uint8_t i);
void printstatus2(struct Locsolo *locsol,uint8_t i);
time_t getTime();
void periodic_timer(); 
void DHT_sensor_read(struct Locsolo *locsol,uint8_t number);
void auto_ontozes(struct Locsolo *locsol,uint8_t number);
void client_login(String *request,struct Locsolo *locsol, WiFiClient *client,uint8_t number);
void load_default(struct Locsolo *locsol,uint8_t number);
void reset_cmd();
void OTAhandle();
void Telnet_print(uint8_t n,...);

typedef struct
{
  short hour;
  short minute;
  short second;
  short weekday;
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

const char* ssid = "wifi";
const char* password = "";
 
WiFiServer server(80);
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP udp;  // A UDP instance to let us send and receive packets over UDP
#include "HTMLpage.h"
extern "C" {
#include "user_interface.h"
}
uint16_t dT;
/*-------------------------------------Telnet Debug----------------------------------------------------------------------------------------*/
  WiFiServer  TelnetDebug(18266);
  WiFiClient  TelnetDebugClient;
/*-------------------------------------Telnet Debug----------------------------------------------------------------------------------------*/
#if OTA_WEB_BROWSER
  ESP8266WebServer httpServer(8266);
  ESP8266HTTPUpdateServer httpUpdater;
#endif
void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.print(F("\n"));Serial.println(sizeof(locsolo[0]));
  Serial.println(sizeof(locsolo));
  Serial.println(sizeof(Locsolo));
#if ENABLE_FLASH
  ETS_UART_INTR_DISABLE();
  if(sizeof(locsolo)>4096){
    spi_flash_read(mem_sector0,(uint32_t *)&locsolo[0],4096);
    spi_flash_read(mem_sector1,(uint32_t *)(&locsolo[0]+4096),LOCSOLO_NUMBER*sizeof(locsolo[0])-4096);
  }else
    {
    spi_flash_read(mem_sector0,(uint32_t *)&sensor,LOCSOLO_NUMBER*sizeof(locsolo[0]));
    } 
  delay(10);
  if(sizeof(sensor)>4096){
    spi_flash_read(mem_sector2,(uint32_t *)&sensor,4096);
    spi_flash_read(mem_sector3,(uint32_t *)(&sensor+4096),sizeof(sensor)-4096);
  }else
    {
    spi_flash_read(mem_sector2,(uint32_t *)&sensor,sizeof(sensor));
    }                                                                             
  ETS_UART_INTR_ENABLE();
#endif

  Serial.println(sizeof(sensor));
  Serial.println();  // Connect to WiFi network
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  uint8_t count=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    count++;
    if(count>100) {
      delay(1000);
      ESP.restart();
    }
  }

  Serial.println(F(""));
  Serial.println(F("WiFi connected"));

/*-----------------------------------------OTA---------------------------------------------------------------------------------------------*/
#if OTA_ARDUINO_IDE  
 ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
#endif
#if OTA_WEB_BROWSER
    httpUpdater.setup(&httpServer);
    httpServer.begin();
#endif
/*-----------------------------------------OTA---------------------------------------------------------------------------------------------*/

/*-------------------------------------Telnet Debug----------------------------------------------------------------------------------------*/
#if TELNET_DEBUG  
  TelnetDebug.begin();
  TelnetDebug.setNoDelay(true);
#endif
/*-------------------------------------Telnet Debug----------------------------------------------------------------------------------------*/
  server.begin();     // Start the server
  Serial.println(F("Server started"));
  dht.begin();              // initialize temperature sensor
  udp.begin(localPort);     //NTP time port
  setSyncProvider(getTime);
  setTime(getTime());
  setSyncInterval(TIME_SYNC_INTERVAL);
  timer1.every(TIMER_PERIOD*1000, periodic_timer);

  Serial.print(F("Free Heap Size:")); Serial.println(system_get_free_heap_size());
  Serial.setDebugOutput(true);
  strcpy(locsolo[0].Name,"locsolo1");
  strcpy(locsolo[1].Name,"locsolo2");
  strcpy(locsolo[2].Name,"locsolo3");
  Serial.print(F("sensor.count:")); Serial.println(sensor.count);
  Serial.print(F("locsolo.count:")); Serial.println(locsolo[0].count);
  if(sensor.count>=SENSOR_MEMORY)  sensor.count=0;
  if(locsolo[0].count>=LOCSOL_MEMORY) locsolo[0].count=0;
  WiFi.printDiag(Serial);
  Serial.print(F("heap size: "));
  Serial.println(ESP.getFreeHeap());
  Serial.print(F("free sketch size: "));
  Serial.println(ESP.getFreeSketchSpace());
} 

void loop() { 
  delay(20);
  #if OTA_ARDUINO_IDE  
    ArduinoOTA.handle();
  #endif
  #if OTA_WEB_BROWSER
    httpServer.handleClient();
  #endif
  #if TELNET_DEBUG
    TelnetDebugHandle();
  #endif
  timer1.update();
  if(timer_flag)
  {
    Serial.print(F("sensor.count:")); Serial.println(sensor.count);
    Serial.print(F("Locsolo.count:")); Serial.println(locsolo[0].count);
    Serial.print(F("Periodic Timer:"));  Serial.print(TIMER_PERIOD);  Serial.println(F("s"));
    time_out(&locsolo[0],LOCSOLO_NUMBER);
    if(now()< 946684800) setTime(getTime());
    else {
      DHT_sensor_read(&locsolo[0],LOCSOLO_NUMBER);
      auto_ontozes(&locsolo[0],LOCSOLO_NUMBER);
      if(flag==0){
        now();
        server_start.second=second();
        server_start.minute=minute();
        server_start.hour=hour();
        flag=1;
        }   
    }
    if(WiFi.status() != WL_CONNECTED) wifinc_count++;
    else {wifinc_count=0; Serial.print(F("wifinc_count=")); Serial.println(wifinc_count);}
    if(wifinc_count>5)  {Serial.println(F("No wifi connection")); sensor.wifi_reset++; reset_cmd();}
    if (TelnetDebugClient.connected()) {String debug_str=F("Time:"); debug_str+=hour(); debug_str+=":"; debug_str+=minute(); debug_str+=":"; debug_str+=second();
                                        debug_str+=F("   Heap size:"); debug_str+=ESP.getFreeHeap(); debug_str+=F(" wifi_reset="); debug_str+=sensor.wifi_reset; 
                                        TelnetDebugClient.println(debug_str);}
//    if (TelnetDebugClient.connected())  Telnet_print("Time:*",hour());
    Serial.printf("heap size: %u\n", ESP.getFreeHeap());
    Serial.println(F("Periodic Timer end"));
    timer_flag=0;
  }
  WiFiClient client;
  client = server.available();
  if (!client) {                //Ha nincs kliens csatlakozva
   client.flush(); client.stop();
   return;
  }
  Serial.println(F("p:2"));
  while(!client.available())   {
    Serial.print(F("."));    
    delay(1);   
    timeout++;
    if(timeout>500) {Serial.println(F("INFINITE LOOP BREAK!"));  client.flush(); client.stop(); Serial.println(F("BREAK NOW!")); break;}
    }
    timeout=0;
    String request = client.readStringUntil('\r');  // Read the first line of the request
    Serial.println(request);
    auto_ontozes(&locsolo[0],LOCSOLO_NUMBER);
    client_login(&request,&locsolo[0],&client,LOCSOLO_NUMBER);
    delay(100);
    if (request.indexOf("/locs1=on") != -1)  {
      locsolo[0].set = HIGH;
      Serial.print(F("locsolo[0]="));
      Serial.println(locsolo[0].set);
      html_index(&client,&locsolo[0]);
    }
      else if (request.indexOf("/locs1=off") != -1)  {
      locsolo[0].set = LOW;
      Serial.print(F("locsolo[0]="));
      Serial.println(locsolo[0].set);
      html_index(&client,&locsolo[0]);
    }
     else if (request.indexOf("/locs1=auto") != -1)  {
      if(locsolo[0].autom) locsolo[0].autom = 0;
      else locsolo[0].autom = 1;
      Serial.print(F("locsolo[0]="));
      Serial.println(locsolo[0].autom);
      html_index(&client,&locsolo[0]);
    }
      else if (request.indexOf("/settings") != -1)  {
      Serial.print(F("Settings"));
      html_settings(&client);
    }
    else if (request.indexOf("erase") != -1)  {
      Serial.println(F("Load defaults"));
      load_default(&locsolo[0],LOCSOLO_NUMBER);
      html_index(&client,&locsolo[0]);
    }
    else if (request.indexOf("S:") != -1)  {                        //ha a settingsnel beallitok valamit
      Serial.println(F("p:8"));
      uint16_t response;
      request.remove(0,request.indexOf("S:")+2);                                          //a formatum peldaul S:00000101
      Serial.println(request);
      response=request.toInt();
      Serial.print(F("Settings response:")); Serial.println(response);
      if(response & (1<<0)) sensor.temperature_graph=1;
        else  sensor.temperature_graph=0;
      if(response & (1<<1)) sensor.humidity_graph=1;
        else  sensor.humidity_graph=0;
      for(int i=0;i<LOCSOLO_NUMBER;i++){      
        if(response & (1<<(2+(2*i)))) locsolo[i].temperature_graph=1;
        else  locsolo[i].temperature_graph=0;
        if(response & (1<<(3+(2*i)))) locsolo[i].voltage_graph=1;
        else  locsolo[i].voltage_graph=0;
        Serial.print(F("temperature_graph:"));Serial.print(locsolo[i].temperature_graph);
        Serial.print(F("voltage_graph:"));Serial.println(locsolo[i].voltage_graph);
      }
      html_settings(&client);
    }
    
    else if (request.indexOf("properties") != -1)  {                        //peldaul dur:200 hour:8 min:15 dur:200 hour:8 min:15  --szimetrikus kovetkezo locsolohoz tartozik
      Serial.println(F("p:20"));
      uint16_t dur,hou,mi;
      String r;
      uint16_t w;
      for(int i=0;i<LOCSOLO_NUMBER;i++){
      request.remove(0,request.indexOf("=")+1);
      r = request.substring(0,request.indexOf("&"));
      r.toCharArray(locsolo[i].alias, r.length()+1); 
     // Serial.print("alias1:"); Serial.println(r);

      //Serial.print("alias1:"); Serial.println(te);
      request.remove(0,request.indexOf("=")+1);
      r=request.substring(0,request.indexOf("&")); w=r.toInt();
      locsolo[i].auto_watering_time.hour=w;
      //Serial.print("hour:"); Serial.println(w);
      request.remove(0,request.indexOf("=")+1);
      r=request.substring(0,request.indexOf("&")); w=r.toInt();
      locsolo[i].auto_watering_time.minute=w;
      //Serial.print("minute:"); Serial.println(w);
      request.remove(0,request.indexOf("=")+1);
      r=request.substring(0,request.indexOf("&")); w=r.toInt();
      locsolo[i].duration=w*60;
     // Serial.print("duration:"); Serial.println(w);
      }
      html_settings(&client);
    }
    else if (request.indexOf("/status") != -1)  {
     Serial.println(F("p:7"));
     status_respond(&client,&locsolo[0],LOCSOLO_NUMBER);
    }
      
    else if (request.indexOf("/reset_reason") != -1) {Serial.println(ESP.getResetReason()); client.println(ESP.getResetReason());}
    else if (request.indexOf("/dht_status") != -1) dht_status(&client);
    else if (request.indexOf("/reset") != -1) reset_cmd();
    else if (request.indexOf("/update") != -1) {}
    #if  ENABLE_IP  
      else if(request.indexOf("/who") != -1)  {
      who_is_connected_HTML(&adress[0],&client);
      }
    #endif
      else if(request.indexOf("GET / HTTP") != -1)  html_index(&client,&locsolo[0]);     // Return the response
    #if  ENABLE_IP
      request = client.readStringUntil('\r');  // Read the request
      who_is_connected(&adress[0],&request);
    #endif
    client.flush();
    delay(1);
    client.stop();
    Serial.println(F("p:5"));
    Serial.println(F("Client disonnected\n"));
}
 //----------------------------------- loop end------------------------------------------------
#if  ENABLE_IP
void who_is_connected(struct Adress *adr, String *s)
{
  //Serial.print(s->charAt(1)); Serial.print(s->charAt(2)); Serial.print(s->charAt(3)); Serial.println(s->charAt(4));
  if(s->charAt(1) == 'H' && s->charAt(2) == 'o' && s->charAt(3) == 's' && s->charAt(4) == 't')
    {
    adr[adr->count].IP = s->substring(6);
    //Serial.print("Connected IP:");
   //Serial.println(adr[adr->count].IP);
    adr[adr->count].epoch=now();
    if(adr[adr->count-1].IP != adr[adr->count].IP || (now()-adr[adr->count-1].epoch)>IP_TIMEOUT)  adr->count++;
    if(adr->count>10)  adr->count=0;
    }
    return;
}
#endif

void DHT_sensor_read(struct Locsolo *locsol,uint8_t number)
{
  float m,n;
  uint8_t i=0;
  Serial.print(F("Time: ")); Serial.print(hour()); Serial.print(F(":")); Serial.print(minute()); Serial.print(F(":")); Serial.println(second());
  do{
      m=dht.readHumidity();
      delay(500);
      Serial.println(F("reading humidity"));
      i++;
    }while(isnan(m) && i<3);
  i=0;
  do{
      n=dht.readTemperature();
      delay(500);
      Serial.println(F("reading tempeature"));
      i++;
    }while(isnan(n) && i<3);
  if(!(isnan(m) || isnan(n)))
  {
    sensor.count_dht++;
    sensor.humidity_measured += m;
    sensor.temperature_measured += n;
  } else Serial.println(F("Failed to read from DHT sensor!"));
  yield();
  Serial.print(F("Humidity: ")); Serial.println(m);
  Serial.print(F("Temperature: ")); Serial.println(n);
  Serial.print(F("DHT count: "));   Serial.println(sensor.count_dht);
  if(sensor.count_dht >= DHT_AVARAGE){
    sensor.temperature_avg=sensor.temperature_measured/sensor.count_dht; 
    sensor.humidity_avg=sensor.humidity_measured/sensor.count_dht;
    sensor.avg_now+=sensor.temperature_avg;  
    sensor.temperature_measured=0;
    sensor.humidity_measured=0;    
    sensor.count_dht=0;
    if((now() - sensor.epoch_now) > 65535) load_default(&locsolo[0],LOCSOLO_NUMBER);   //Ha 2^16 = 65535 ~ 18,2 oratol több idő telt el túlcsordulás történik és jobb ha resetelve van minden, a későbbiekben talán jobb nem lenullázni mindent pl. beállítások
    for(int i=SENSOR_MEMORY-1;i>0;i--) {                          //Ha már elértem fogom a hőmérsékleti változókat és egyel odébb dobok mindent úgy hogy az
      sensor.temperature_saved[i] = sensor.temperature_saved[i-1];    //az legkésőbbi érték elveszik
      sensor.humidity_saved[i]    = sensor.humidity_saved[i-1];
      sensor.epoch_saved_dt[i]    = sensor.epoch_saved_dt[i-1];
      }
    sensor.epoch_saved_dt[0]    = now() - sensor.epoch_now;
    sensor.temperature_saved[0] = sensor.temperature_avg*10;
    sensor.humidity_saved[0]    = sensor.humidity_avg;
    sensor.epoch_now=now();
    if(sensor.count<(SENSOR_MEMORY-1))  sensor.count++;                    //Ha még nem értem el a ESP8266 memória végét
    sensor.daily_avg_cnt++;      //Napi átlaghőmérséklethez kell
    for(int i=0;i<number;i++){                //Ha a kliens nem jelentkezik be akkor a klien feszültség és hőmérséklet értékei az előző értéket veszik fel
      for(int j=LOCSOL_MEMORY-1;j>=0;j--){
        locsol[i].voltage[j]  = locsol[i].voltage[j-1];
        locsol[i].temp[j]     = locsol[i].temp[j-1];
      }
      if(locsol[i].count<(LOCSOL_MEMORY-1)) locsol[i].count++;              //Ha még nem értem el a ESP8266 memória végét
    }
    if(locsol[0].count>2){                                                                                            //Locsolo napi minimumok es maximumok
      for(int i=0;i<LOCSOLO_NUMBER;i++) {                                                                          //megallapitasa
      if(locsol[i].temp[locsol[i].count-1]<locsol[i].temp_min) locsol[i].temp_min=locsol[i].temp[locsol[i].count-1];
      if(locsol[i].temp[locsol[i].count-1]>locsol[i].temp_max) locsol[i].temp_max=locsol[i].temp[locsol[i].count-1];
      }
    }
    if(sensor.temperature_avg != 0 && sensor.temperature_avg>sensor.Max) {sensor.Max=sensor.temperature_avg; }                              //Napi extrem hőmérsékletek megállapítása
    if(sensor.temperature_avg != 0 && sensor.temperature_avg<sensor.Min) {sensor.Min=sensor.temperature_avg; }
  }
  Serial.print(F("Writing flash memory:"));                          //FLASH Memory save
#if ENABLE_FLASH
  ETS_UART_INTR_DISABLE();
  if(sizeof(locsolo)>4096){
    spi_flash_erase_sector(mem_sector0>>12);
    spi_flash_write(mem_sector0,(uint32_t *)&locsolo[0],4096);
    spi_flash_erase_sector(mem_sector1>>12);
    spi_flash_write(mem_sector1,(uint32_t *)(&locsolo[0]+4096),LOCSOLO_NUMBER*sizeof(locsolo[0])-4096);
  }else
    {
    spi_flash_erase_sector(mem_sector0>>12);
    spi_flash_write(mem_sector0,(uint32_t *)&sensor,LOCSOLO_NUMBER*sizeof(locsolo[0]));
    }
    if(sizeof(sensor)>4096){
      spi_flash_erase_sector(mem_sector2>>12);
      spi_flash_write(mem_sector2,(uint32_t *)&sensor,4096);
      spi_flash_erase_sector(mem_sector3>>12);
      spi_flash_write(mem_sector3,(uint32_t *)(&sensor+4096),sizeof(sensor)-4096);
    }else
      {
      spi_flash_erase_sector(mem_sector2>>12);
      spi_flash_write(mem_sector2,(uint32_t *)&sensor,sizeof(sensor));
      }
    ETS_UART_INTR_ENABLE();  
#endif
  if(day()!=sensor.thisday)  {
                                  sensor.Max=-273;  sensor.Min=999;
                                  sensor.avg_previous=sensor.avg_now/sensor.daily_avg_cnt; sensor.daily_avg_cnt=0;  sensor.avg_now=0;
                                  for(int i=0;i<LOCSOLO_NUMBER;i++) {                                                                          //megallapitasa
                                      locsol[i].temp_min=999;   //atlaggal helzetesiteni majd
                                      locsol[i].temp_max=-273;  //atlaggal helyetesiteni majd
                                      
                                      }
                                  sensor.thisday=day();
                                  //Serial.print(F("   sensor.avg_previous:")); Serial.println(sensor.avg_previous); 2016.1.18 Delete if OK!
  }
  if(hour()==11 && minute()<5             ) {sensor.avg_nr=1; sensor.avg_3h_temp=sensor.temperature_avg;}              //kiszámolom a 11 óra,13 óra és 15 óra körül mért hőmérséklet átlagát.
  if(hour()==13 && minute()<5 && sensor.avg_nr==1) {sensor.avg_nr++; sensor.avg_3h_temp+=sensor.temperature_avg;}
  if(hour()==15 && minute()<5 && sensor.avg_nr==2) {sensor.avg_nr++; sensor.avg_3h_temp+=sensor.temperature_avg;  sensor.avg_3h=sensor.avg_3h_temp/3; sensor.avg_nr=0; water_points(&locsolo[0]);} //megállapítom kell-e öntözni
  Serial.print(F("Debug, avg_nr=")); Serial.print(sensor.avg_nr); Serial.print(F("  temp_avg="));  Serial.println(sensor.avg_3h); //2016.1.18 Delete if OK!
}

void water_points(struct Locsolo *locsol)
{
    Serial.println(F("Debug---3:"));
    Serial.println(sensor.avg_3h);
    uint8_t flag=0;
    for(int i=0;i<LOCSOLO_NUMBER;i++) if(locsol[i].autom == 1) {flag=1; break;}
    if(flag==1)
    {
    if(sensor.avg_3h<15)                      sensor.water_points_increase=1;
    if(15<sensor.avg_3h && sensor.avg_3h<20)  sensor.water_points_increase=2;
    if(20<sensor.avg_3h && sensor.avg_3h<25)  sensor.water_points_increase=3;
    if(25<sensor.avg_3h && sensor.avg_3h<30)  sensor.water_points_increase=4;
    if(30<sensor.avg_3h && sensor.avg_3h<35)  sensor.water_points_increase=5;
    if(35<sensor.avg_3h)                      sensor.water_points_increase=6;
    sensor.water_points+=sensor.water_points_increase;  
    }
    else sensor.water_points_increase=0;
}

void auto_ontozes(struct Locsolo *locsol,uint8_t number){
  for(int l=0;l<number;l++){
    if(locsol[l].autom==1 && sensor.water_points>=7 && (hour()==locsol[l].auto_watering_time.hour || (hour()-1)==locsol[l].auto_watering_time.hour) && (minute()>=locsol[l].auto_watering_time.minute || (hour()-1)==locsol[l].auto_watering_time.hour) && locsol[l].set==0)
    {
      Serial.println(F("AUTO locsolas"));
      locsol[l].set=1; 
      locsol[l].auto_flag=1;
//    locsol[l].watering_epoch=now();
//    locsol[i].auto_watering_time.second=second(); locsol[i].auto_watering_time.minute=minute(); locsol[i].auto_watering_time.hour=hour(); locsol[i].auto_watering_time.weekday=weekday();
/*#if ENABLE_FLASH
    ETS_UART_INTR_DISABLE();
  if(sizeof(locsolo)>4096){
    spi_flash_erase_sector(mem_sector0>>12);
    spi_flash_write(mem_sector0,(uint32_t *)&locsolo[0],4096);
    spi_flash_erase_sector(mem_sector1>>12);
    spi_flash_write(mem_sector1,(uint32_t *)(&locsolo[0]+4096),LOCSOLO_NUMBER*sizeof(locsolo[0])-4096);
  }else
    {
    spi_flash_erase_sector(mem_sector0>>12);
    spi_flash_write(mem_sector0,(uint32_t *)&locsolo[0],LOCSOLO_NUMBER*sizeof(locsolo[0]));
    }
    if(sizeof(sensor)>4096){
      spi_flash_erase_sector(mem_sector2>>12);
      spi_flash_write(mem_sector2,(uint32_t *)&sensor,4096);
      spi_flash_erase_sector(mem_sector3>>12);
      spi_flash_write(mem_sector3,(uint32_t *)(&sensor+4096),sizeof(sensor)-4096);
    }else
      {
      spi_flash_erase_sector(mem_sector2>>12);
      spi_flash_write(mem_sector2,(uint32_t *)&sensor,sizeof(sensor));
      }
    ETS_UART_INTR_ENABLE();  
#endif*/
    }
    if((now()-locsol[l].watering_epoch)>locsol[l].duration && sensor.water_points>=7 && locsol[l].set==1 && locsol[l].auto_flag==0)
    {
      Serial.println(F("AUTO locsolas vége"));
      locsol[l].set=0; 
      sensor.water_points-=7;
    }  
  }
}

void periodic_timer()                                                                     //This function is called periodically
{
  timer_flag=1;                                                                           //Set flag, this flag executes some other function from main loop. 
}                                                                                         //Its needed because interrupt function cannot be too long, it can cause wdt reset

void client_login(String *request,struct Locsolo *locsol, WiFiClient *client,uint8_t number)  //Ha kapcsolodik a kliens ez a kĂłdsor fog lefutni
{
  for(int i=0;i<number;i++){
    if (request->equals(locsol[i].Name))  {              //a kliens azonositja magát; locsolo1,locsolo2,locsolo3?
      printstatus1(locsol,i);                           //a soros porton kiírom ki csatlakozott
      Serial.println("1");
      //delay(10);
      if(client->connected())   client->print(locsol[i].Name);                    //a kliensnek visszaküldöm a nevét, majd az egyenlőségjel után kiküldöm milyen állapotban kell lennie
      if(client->connected())   client->println("=");
      Serial.println("2");
      //delay(10);
      if(client->connected())   client->println(locsol[i].set);
      //delay(10);
      Serial.println("3");
      *request = client->readStringUntil('\r');        //a kliens visszaküldi milyen állapotban van
      Serial.println("4");
      if(*request==0) {locsol[i].temp[locsol[i].count];  return;}
      locsol[i].state=request->toInt();
      locsol[i].state--;
      *request = client->readStringUntil('\r');        //a kliens visszaküldi a mért feszültséget
      Serial.println("5");
      if(*request==0) {Serial.println("Connection error");  return;}
      delay(1);
      locsol[i].voltage[0]=request->toInt();
      *request = client->readStringUntil('\r');        //a kliens visszaküldi a mért hőmérsékletet
      Serial.println("6");
      if(*request==0) {Serial.println("Connection error");  return;}
      delay(1);
      locsol[i].temp[0]=request->toFloat()*10;
      *request = client->readStringUntil('\r');        //a kliens visszaküldi a mért páratartalmat
      if(*request==0) {Serial.println("Connection error");  return;}
      delay(1);
      locsol[i].humidity=request->toInt();
      
      //if(locsol[i].count>LOCSOL_MEMORY) locsol[i].count=0;  //delete line if OK
      printstatus2(locsol,i);                           //a soros portra kiírom milyen állapotban van a kliens
      if(now()> 946684800) locsol[i].login_epoch=now();                           //mentem a csatlakozási időadatokat
      locsol[i].timeout=0;                              //ha sokaig nincs csatlakozott kliens a timeout jelez 1-es értékkel
      if(client->connected())   client->stop();
      if(now()> 946684800 && locsol[i].auto_flag==1) {
        locsol[i].watering_epoch=now();
        locsol[i].auto_flag=0;
#if ENABLE_FLASH
        ETS_UART_INTR_DISABLE();
        if(sizeof(locsolo)>4096){
          spi_flash_erase_sector(mem_sector0>>12);
          spi_flash_write(mem_sector0,(uint32_t *)&locsolo[0],4096);
          spi_flash_erase_sector(mem_sector1>>12);
          spi_flash_write(mem_sector1,(uint32_t *)(&locsolo[0]+4096),LOCSOLO_NUMBER*sizeof(locsolo[0])-4096);
        }
        else{
          spi_flash_erase_sector(mem_sector0>>12);
          spi_flash_write(mem_sector0,(uint32_t *)&locsolo[0],LOCSOLO_NUMBER*sizeof(locsolo[0]));
        }
        if(sizeof(sensor)>4096){
          spi_flash_erase_sector(mem_sector2>>12);
          spi_flash_write(mem_sector2,(uint32_t *)&sensor,4096);
          spi_flash_erase_sector(mem_sector3>>12);
          spi_flash_write(mem_sector3,(uint32_t *)(&sensor+4096),sizeof(sensor)-4096);
        }
        else{
          spi_flash_erase_sector(mem_sector2>>12);
          spi_flash_write(mem_sector2,(uint32_t *)&sensor,sizeof(sensor));
        }
        ETS_UART_INTR_ENABLE();  
#endif
        }
    }
  }
}

struct Locsolo printstatus1(struct Locsolo *locsol,uint8_t i)
{
  Serial.print(F("ESP8266_server reports "));
  Serial.print(locsol[i].Name);
  Serial.println(F(" is connected."));
  Serial.print(F("ESP8266_server reports "));
  Serial.print(locsol[i].Name);
  Serial.print(F("_set="));
  Serial.print(locsol[i].set);
}
void printstatus2(struct Locsolo *locsol,uint8_t i)
{
  Serial.print(F("ESP8266_server reports")); 
  Serial.print(locsol[i].Name);
  Serial.print(F("_status="));
  Serial.println(locsol[i].state);
  Serial.print(F("Voltage: "));
  Serial.println(locsol[i].voltage[locsol[i].count]);
  Serial.print(F("Temperature: "));
  Serial.println(locsol[i].temp[locsol[i].count]);
  Serial.print(F("Humidity: "));
  Serial.println(locsol[i].humidity);
}

void load_default(struct Locsolo *locsol,uint8_t number)
{
 for(int i=0;i<number;i++){
   locsolo[i].count=0; 
   locsolo[i].login_epoch=0; 
   locsolo[i].watering_epoch=0;
   locsolo[i].set=LOW;            //Állapot amire állítom
   locsolo[i].state=LOW;          //Állapot amiben jelenleg van
   locsolo[i].autom=LOW;          //Automata öntözés be-ki kapcsolása
   locsolo[i].timeout=LOW;
   locsolo[i].humidity=0;
   locsolo[i].temp_max=-273;
   locsolo[i].temp_min=999;
   locsolo[i].temperature_graph=HIGH;
   locsolo[i].voltage_graph=HIGH;
   locsol[i].auto_watering_time.hour=7;
   locsol[i].auto_watering_time.minute=1;
   locsol[i].duration=600;
   locsolo[i].auto_flag=0;
   locsolo[i].alias[0]='\n';
   strcpy(locsolo[i].alias,"ext.sensor1");
   locsolo[i].alias[10]=48+i+1;
   locsolo[i].short_name=i+65;
   for(int j=0;j<LOCSOL_MEMORY;j++)
   {
    locsolo[i].voltage[j]=0;
    locsolo[i].temp[j]=0;
    }
 }
 sensor.temperature_avg=0;
 sensor.temperature_measured=0;
 sensor.avg_previous=0;
 sensor.avg_3h=0;
 sensor.humidity_avg=0;
 sensor.avg_now=0;
 sensor.daily_avg_cnt=0;
 sensor.Min=999;  
 sensor.Max=-273.15;
 sensor.humidity_measured=0;
 sensor.humidity_min=0;  
 sensor.humidity_max=0;
 sensor.water_points=7;
 sensor.water_points_increase=0;
 sensor.count=0;
 sensor.count_dht=0;
 sensor.temperature_graph=HIGH;
 sensor.humidity_graph=HIGH;
 sensor.thisday=0;
 sensor.wifi_reset=0;
}
void time_out(struct Locsolo *locsol, uint8_t number)
{
  for(int i=0;i<number;i++){
    if(now()-locsol[i].login_epoch>LOGIN_TIME_OUT)
    {
    locsol[i].timeout=1;
    Serial.print(locsol[i].Name);
    Serial.println(F(" time out"));
    }
  }
 }
void reset_cmd()
{
  Serial.println(F("---------reset command---------")); delay(100);
  ESP.restart();
}

void TelnetDebugHandle(){
  if (TelnetDebug.hasClient()){
    //find free/disconnected spot
      if (!TelnetDebugClient || TelnetDebugClient.connected()){
        if(TelnetDebugClient) TelnetDebugClient.stop();
        TelnetDebugClient = TelnetDebug.available();
        Serial1.print("New client: ");
        delay(500);
        TelnetDebugClient.println("Welcome!");
        delay(500);
      }
    //no free/disconnected spot so reject
    WiFiClient serverClient = server.available();
    serverClient.stop();
  }
  if(!TelnetDebugClient.connected()) TelnetDebugClient.stop();
}
/*********DEBUG via TELNET***********************************************************************
 * 
 * 
 * 
 */

 /*
void Telnet_print(uint8_t n,...)
{
  String data;
  int i;
  va_list lista;
  data[0]=0;
  va_start(lista, n);
  for(i=0;i<n;i++)
  {
    data += (String)va_arg(lista, char);
  }
  TelnetDebugClient.println(data);
}

*/
// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println(F("sending NTP packet..."));
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

time_t getTime()
{
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println(F("no packet yet"));
  }
  else {
    Serial.print(F("packet received, length="));
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print(F("Seconds since Jan 1 1900 = "));
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print(F("Unix time = "));
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    if(flag==1) {Serial.println(F("DEBUG now()")); epoch_previous=now();}
    Serial.print(F("DEBUG, epoch_previous:")); Serial.println(epoch_previous);
    unsigned long epoch = secsSince1900 - seventyYears;
    if(epoch < 946684800)  epoch=epoch_previous;
    // print Unix time:
    Serial.println(epoch);

    // print the hour, minute and second:
    Serial.print(F("The UTC time is "));       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second
    
    if(month()>10 || month()<4)   epoch = secsSince1900 - seventyYears + TIME_ZONE * SECS_PER_HOUR;   //Convert to time zone and daylight saving
    else                          epoch = secsSince1900 - seventyYears + (TIME_ZONE + DAYLIGHT_SAVING) * SECS_PER_HOUR;   //Convert to time zone
    Serial.println("return epoch");
    return epoch;
  }
}




