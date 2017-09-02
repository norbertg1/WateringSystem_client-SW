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
#include <PubSubClient.h>

//----------------------------------------------------------------settings---------------------------------------------------------------------------------------------------------------------------------------------//
#define Debug_Voltage                     0
#define SLEEP_TIME_NO_WIFI_SECONDS        120                             //when cannot connect to saved wireless network in seconds, this is the time until we can set new SSID
#define MAX_VALVE_SWITCHING_TIME_SECONDS  15                              //The time when valve is switched off in case of broken microswitch or mechanical failure
//---------------------------------------------------------------End of settings---------------------------------------------------------------------------------------------------------------------------------------//

//------------------------------------------------------------------------Do not edit------------------------------------------------------------------------------------------------
#define SLEEP_TIME_NO_WIFI                SLEEP_TIME_NO_WIFI_SECONDS * 1000000  //when cannot connect to saved wireless network, this is the time until we can set new wifi SSID
#define SLEEP_TIME                        sleep_time_seconds * 1000000          //when watering is off, in microseconds
#define DELAY_TIME                        delay_time_seconds * 1000             //when watering is on, in miliseconds
#define MAX_VALVE_SWITCHING_TIME          MAX_VALVE_SWITCHING_TIME_SECONDS*1000
#define DHT_PIN                           0
#define DHT_TYPE                          DHT22
#define LOCSOLO_NUMBER                    2
#define VALVE_H_BRIDGE_RIGHT_PIN          12
#define VALVE_H_BRIDGE_LEFT_PIN           14
#define VALVE_SWITCH_ONE                  4
#define VALVE_SWITCH_TWO                  13
#define ADC_SWITCH                        15
#define VOLTAGE_TO_DIVIDER                3
#define MQTT_SERVER                       "locsol.dynamic-dns.net"
//--------------------------------------------------------------------End----------------------------------------------------------------------------------------------------------------------------------------------------//
const char* ssid     = "wifi";
const char* password = "";

const char* host = "192.168.1.100";

DHT dht(DHT_PIN,DHT_TYPE);
ESP8266WebServer server ( 80 );
Ticker Voltage_Read;            //Install periodic timer that in specified time reads battery voltage. ADC in must be unconnected!!!
BMP280 bmp;
WiFiClient espClient;
PubSubClient client(espClient);
ADC_MODE(ADC_VCC); //only for old design

uint32_t voltage,moisture;
double T,P;
uint8_t hum;
float temp,temperature;
int locsolo_state=LOW, on_off_command=LOW;
uint16_t  locsolo_duration;
uint16_t  locsolo_start;
short locsolo_flag=0;
short locsolo_number = LOCSOLO_NUMBER - 1;
char device_name[]="locsolo1";
int sleep_time_seconds=900;                     //when watering is off, in seconds
int delay_time_seconds=60;                     //when watering is on, in seconds

void valve_turn_on();
void valve_turn_off();
int valve_state();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();

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
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(mqtt_callback);

  int i=0;
  while(i<600){
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
      //ESP.deepSleep(0);
    }
  }
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  MDNS.begin("watering_client1");
  MDNS.addService("watering_server", "tcp", 8080);
  if(!bmp.begin())  {Serial.println("BMP init failed!");    bmp.setOversampling(16);}
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
  for(int j=0;j<50;j++) voltage+=ESP.getVcc(); // voltage+=analogRead(A0); for new design
  digitalWrite(VOLTAGE_TO_DIVIDER, 0);
  voltage=(voltage/50);//*5.7;                           //5.7 is the resistor divider value
  Serial.print("Voltage:");  Serial.println(voltage); 

  digitalWrite(ADC_SWITCH, 1);
  delay(200);
  moisture=0;
  for(int j=0;j<50;j++) moisture+=analogRead(A0);
  moisture=(((float)moisture/50)/1024.0)*100;
  Serial.print("Moisture:");  Serial.println(moisture);
  double t=0,p=0;
  delay(bmp.startMeasurment());
  bmp.getTemperatureAndPressure(T,P);
  for(int i=0;i<5;i++){
    delay(bmp.startMeasurment());
    bmp.getTemperatureAndPressure(T,P);
    t+=T;
    p+=P;
  }
  T=t/5;
  P=p/5;
  Serial.print("T=");     Serial.print(T);
  Serial.print("   P=");  Serial.println(P);
  
  mqtt_reconnect();
  char buf_name[50];                                                    //berakni funkcioba szepen mint kell
  char buf[10];
  client.loop(); 
  dtostrf(T,6,1,buf);
  sprintf (buf_name, "%s%s", device_name,"/TEMPERATURE");
  client.publish(buf_name, buf);
  itoa((float)moisture, buf, 10);
  sprintf (buf_name, "%s%s", device_name,"/MOISTURE");
  client.publish(buf_name, buf);
  dtostrf((float)voltage/1000,6,3,buf);
  sprintf (buf_name, "%s%s", device_name,"/VOLTAGE");
  client.publish(buf_name, buf);
  dtostrf(P,6,3,buf);
  sprintf (buf_name, "%s%s", device_name,"/PRESSURE");
  client.publish(buf_name, buf);
  sprintf (buf_name, "%s%s", device_name,"/READY_FOR_DATA");
  client.publish(buf_name, "0");
  client.loop();
  for(int i = 0; i<20; i++){
    delay(50);
    client.loop();
  }
  
  if(on_off_command && (float)voltage/1000>3.0)  valve_turn_on();
  else  valve_turn_off();
  if(valve_state() == 0){
    Serial.print("Valve state: "); Serial.println(valve_state());
    Serial.println("Deep Sleep");  
    sprintf (buf_name, "%s%s", device_name,"/ON_OFF_STATE");
    client.publish(buf_name, "0");
    sprintf (buf_name, "%s%s", device_name,"/END");
    client.publish(buf_name, "0");
    delay(100);
    ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);
    delay(100);
  }
  else   {
    Serial.print("Valve state: "); Serial.println(valve_state());
    mqtt_reconnect();
    client.loop(); 
    sprintf (buf_name, "%s%s", device_name,"/ON_OFF_STATE");
    client.publish(buf_name, "1");
    sprintf (buf_name, "%s%s", device_name,"/END");
    client.publish(buf_name, "0");
    client.loop(); 
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
  if(!valve_state) locsolo_state=LOW;
}

void battery_read(){
  Serial.print("Voltage: "); Serial.print((float)ESP.getVcc()/1000); Serial.println("V");
  Serial.print("ADC function Voltage: "); Serial.print((float)analogRead(A0)*5.7); Serial.println("V");  
}

int valve_state(){
  return digitalRead(VALVE_SWITCH_TWO);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char buff[50];
  sprintf (buff, "%s%s", device_name,"/ON_OFF_COMMAND");
  if(!strcmp(topic,buff)) {
    on_off_command=payload[0]-48;
    Serial.print("Valve command: ");  Serial.println(on_off_command);
  }
  sprintf (buff, "%s%s", device_name,"/DELAY_TIME");
  if(!strcmp(topic,buff)) {
    for (int i = 0; i < length; i++) buff[i]=(char)payload[i];
    buff[length] = '\n';
    delay_time_seconds=atoi(buff);
    Serial.print("Delay time_seconds: "); Serial.println(delay_time_seconds);
  }
  sprintf (buff, "%s%s", device_name,"/SLEEP_TIME");
  if(!strcmp(topic,buff)) {
    for (int i = 0; i < length; i++) buff[i]=(char)payload[i];
    buff[length] = '\n';
    sleep_time_seconds=atoi(buff);
    Serial.print("Sleep time_seconds: "); Serial.println(sleep_time_seconds);
  }  
}

void mqtt_reconnect() {
  char buf_name[50];
  if (!client.connected()) {
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    client.connect(clientId.c_str());
    sprintf (buf_name, "%s%s", device_name,"/ON_OFF_COMMAND");
    client.subscribe(buf_name);
    client.loop(); 
    sprintf (buf_name, "%s%s", device_name,"/SLEEP_TIME");
    client.subscribe(buf_name);
    client.loop(); 
    sprintf (buf_name, "%s%s", device_name,"/DELAY_TIME");
    client.subscribe(buf_name);
    client.loop();
  }
}

