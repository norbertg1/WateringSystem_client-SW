 /*
 *  This program login into ESP8266_locsolo_server. Gets the A0 pin status from the server then set it. Also send Vcc voltage and temperature.
 *  When A0 is HIGH ESP8266 loggin in to the serve every 30seconds, if it is LOW goind to deep sleep for 300seconds
 *  Created on 2015.08-2015.11
 *  by Norbi
 *  
 *  3V alatt ne nyisson ki a szelep, de ha nyitva van akkor legyen egy deltaU feszültség ami alatt csukodk be (pl 2.9V)
 *  Ha nincs IP cim (0.0.0.0) vagyis ha nem tudott csatlakozni az wdt resetet eredemnyezthet
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

//----------------------------------------------------------------settings---------------------------------------------------------------------------------------------------------------------------------------------//
#define Debug_Voltage                     0
#define SLEEP_TIME_SECONDS                120//900                             //when watering is off, in seconds
#define DELAY_TIME_SECONDS                60                              //when watering is on, in seconds
#define SLEEP_TIME_NO_WIFI_SECONDS        120                             //when cannot connect to saved wireless network in seconds, this is the time until we can set new SSID
#define MAX_VALVE_SWITCHING_TIME_SECONDS  15                              //The time when valve is switched off in case of broken microswitch or mechanical failure
#define DHT_ENABLE                        0
#define BMP_ENABLE                        1
//---------------------------------------------------------------End of settings---------------------------------------------------------------------------------------------------------------------------------------//

//------------------------------------------------------------------------Do not edit------------------------------------------------------------------------------------------------
#define SLEEP_TIME_NO_WIFI                SLEEP_TIME_NO_WIFI_SECONDS * 1000000  //when cannot connect to saved wireless network, this is the time until we can set new wifi SSID
#define SLEEP_TIME                        SLEEP_TIME_SECONDS * 1000000          //when watering is off, in microseconds
#define DELAY_TIME                        DELAY_TIME_SECONDS * 1000             //when watering is on, in miliseconds
#define MAX_VALVE_SWITCHING_TIME          MAX_VALVE_SWITCHING_TIME_SECONDS*1000
#define DHT_PIN                           0
#define DHT_TYPE                          DHT11
#define LOCSOLO_NUMBER                    2
#define VALVE_H_BRIDGE_RIGHT_PIN          12
#define VALVE_H_BRIDGE_LEFT_PIN           14
#define VALVE_SWITCH_ONE                  4
#define VALVE_SWITCH_TWO                  13
#define ADC_SWITCH                        15
#define VOLTAGE_TO_DIVIDER                3
//--------------------------------------------------------------------End----------------------------------------------------------------------------------------------------------------------------------------------------//
const char* ssid     = "wifi";
const char* password = "";

const char* host = "192.168.1.100";

DHT dht(DHT_PIN,DHT_TYPE);
ESP8266WebServer server ( 80 );
Ticker Voltage_Read;            //Install periodic timer that in specified time reads battery voltage. ADC in must be unconnected!!!
BMP280 bmp;
//ADC_MODE(ADC_VCC);

uint32_t voltage,moisture;
double T,P;
uint8_t hum;
float temp,temperature;
short locsolo_state=LOW;
uint16_t  locsolo_duration;
uint16_t  locsolo_start;
short locsolo_flag=0;
short locsolo_number = LOCSOLO_NUMBER - 1;
 
void valve_turn_on();
void valve_turn_off();
int valve_state();

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(ESP.getResetReason());
  Serial.println("\nESP8266_client start!");
  pinMode(ADC_SWITCH, OUTPUT);
  pinMode(VOLTAGE_TO_DIVIDER, OUTPUT);
  pinMode(VALVE_H_BRIDGE_RIGHT_PIN, OUTPUT);
  pinMode(VALVE_H_BRIDGE_LEFT_PIN, OUTPUT);
  pinMode(VALVE_SWITCH_ONE, INPUT_PULLUP);
  pinMode(VALVE_SWITCH_TWO, INPUT_PULLUP);
  if(valve_state) valve_turn_off();
  WiFiManager wifiManager;
  WiFi.mode(WIFI_STA);
 
  int i=0;
  while(i<200){
    i++;
    delay(100);
    if(i%10==0) Serial.print(".");
    if((WiFi.status()==WL_CONNECTED)) break;
  }
  if(!(WiFi.status()==WL_CONNECTED)){
    wifiManager.setConfigPortalTimeout(120);
    if(!wifiManager.startConfigPortal("ESP8266_client")) {
      Serial.println("Not connected to WiFi but continuing anyway.");
      Serial.println("Deep Sleep");
      ESP.deepSleep(SLEEP_TIME_NO_WIFI,WAKE_RF_DEFAULT);
    }
  }
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  MDNS.begin("watering_client1");
  MDNS.addService("watering_server", "tcp", 8080);
  if(!bmp.begin())  {Serial.println("BMP init failed!");    bmp.setOversampling(4);}
  else              Serial.println("BMP init success!");
  }

void loop() {
  int r,i,len,http_code=0;
  HTTPClient http;
  WiFiClient *stream;
  uint8_t buff[128] = { 0 };
  uint8_t mn=0,count=0;
  
  digitalWrite(ADC_SWITCH, 0);
  digitalWrite(VOLTAGE_TO_DIVIDER, 1);
  delay(200);
  voltage=0;
  for(int j=0;j<50;j++) voltage+=analogRead(A0);
  digitalWrite(VOLTAGE_TO_DIVIDER, 0);
  voltage=(voltage/50)*5.7;                           //5.7 is the resistor divider value
  Serial.print("Voltage:");  Serial.println(voltage); 

  digitalWrite(ADC_SWITCH, 1);
  delay(200);
  moisture=0;
  for(int j=0;j<50;j++) moisture+=analogRead(A0);
  moisture=((moisture/50)/1024.0)*100;
  Serial.print("Moisture:");  Serial.println(moisture);
  
  double t=0,p=0;
  for(int i=0;i<10;i++){
    delay(bmp.startMeasurment());
    bmp.getTemperatureAndPressure(T,P);
    t+=T;
    p+=P;
  }
  T=t/10;
  P=p/10;
  Serial.print("T=");     Serial.print(T);
  Serial.print("   P=");  Serial.println(P);

  ESP.restart();
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  IPAddress IP=WiFi.localIP();
  
  int n=0;
  while(n == 0 && count < 3){
    n=MDNS.queryService("watering_server", "tcp");
    count++;
  }
  Serial.print("mDNS search attemps n = "); Serial.println(count);
  if (n == 0) {
    Serial.println("no services found");
    Serial.println("Deep Sleep");  
    ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);
    delay(100);
  }
  else {
    Serial.print("IP from mDNS:");
    Serial.println(MDNS.IP(0));
  }
  String s="http:";
  s+="//" + String(MDNS.IP(0)[0]) + "." + String(MDNS.IP(0)[1]) + "." + String(MDNS.IP(0)[2]) + "." + String(MDNS.IP(0)[3]);
  s+="/client?=" + String(locsolo_number) + "&=" + String(T*10) + "&=" + String(moisture) + "&=" + String(voltage) + "&=" + IP[0] + "." + IP[1] + "." + IP[2] + "." + IP[3]; 
  Serial.println(s);
  count=0;
  while(http_code!=200 && count < 3){
    http.begin(s);
    http_code=http.GET();
    count++;
    if(http_code != 200)  {delay(5000); Serial.println("cannot connect, reconnecting...");}
  }
  Serial.print("Connecting attemps to server n = "); Serial.println(count);
  len = http.getSize();
  stream = http.getStreamPtr();
  while(http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if(size)  {
      int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
      if(len > 0) len -= c;
    }
  }
  String buff_string;
  for(int k=0; k<20; k++){
      buff_string += String((char)buff[k]);
      }
  Serial.println(buff_string);
  locsolo_state=(buff_string.substring(6,(buff_string.indexOf("_")))).toInt();
  if(locsolo_state && (float)voltage/1000>3.0)  valve_turn_on();
  else  valve_turn_off();
  if(locsolo_state == 0){
    Serial.println("Deep Sleep");  
    http.end();
    delay(100);
    ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);
    delay(100);
  }
  else   {
    Serial.println("delay");
//  WiFi.disconnect();
//  WiFi.forceSleepBegin();
  
    delay(DELAY_TIME);
//  WiFi.forceSleepWake();
//  delay(100); 
  }
}

void valve_turn_on(){
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
  uint32_t t=millis();
  while(!digitalRead(VALVE_SWITCH_TWO) && (millis()-t)<MAX_VALVE_SWITCHING_TIME){
    delay(100);
    }
  if(valve_state) locsolo_state=HIGH;
  }

void valve_turn_off(){
  uint16_t cnt=0;  
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
  uint32_t t=millis();
  while(!digitalRead(VALVE_SWITCH_ONE) && (millis()-t)<MAX_VALVE_SWITCHING_TIME){
    delay(100);
    }
}

void battery_read(){
  Serial.print("Voltage: "); Serial.print((float)ESP.getVcc()/1000); Serial.println("V");
  Serial.print("ADC function Voltage: "); Serial.print((float)analogRead(A0)*5.7); Serial.println("V");  
}

int valve_state(){
  return digitalRead(VALVE_SWITCH_TWO);
}

