/*
   This program login into ESP8266_locsolo_server. Gets the A0 pin status from the server then set it. Also send Vcc voltage and temperature.
   When A0 is HIGH ESP8266 loggin in to the serve every 30seconds, if it is LOW goind to deep sleep for 300seconds
   Created on 2015.08-2015.11
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

//----------------------------------------------------------------settings---------------------------------------------------------------------------------------------------------------------------------------------//
#define WIFI_CONNECTION_TIMEOUT           30                              //Time for connecting to wifi in seconds
#define WIFI_CONFIGURATION_PAGE_TIMEOUT   300                             //when cannot connect to saved wireless network in seconds, this is the time until we can set new SSID in seconds
#define MAX_VALVE_SWITCHING_TIME_SECONDS  15                              //The time when valve is switched off in case of broken microswitch or mechanical failure in seconds
#define WEB_UPDATE_TIMEOUT_SECONDS        180                             //The time out for web update server in seconds 
//---------------------------------------------------------------End of settings---------------------------------------------------------------------------------------------------------------------------------------//

//------------------------------------------------------------------------Do not edit------------------------------------------------------------------------------------------------
#define SLEEP_TIME_NO_WIFI                SLEEP_TIME_NO_WIFI_SECONDS * 1000000    //when cannot connect to saved wireless network, this is the time until we can set new wifi SSID
#define SLEEP_TIME                        sleep_time_seconds * 1000000            //when watering is off, in microseconds
#define DELAY_TIME                        delay_time_seconds * 1000               //when watering is on, in miliseconds
#define WEB_UPDATE_TIMEOUT                WEB_UPDATE_TIMEOUT_SECONDS  * 1000      //time out for web update server
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

DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server ( 80 );
BMP280 bmp;
WiFiClient espClient;
PubSubClient client(espClient);
ADC_MODE(ADC_VCC); //only for old design

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
uint32_t voltage, moisture;
double T, P;
uint8_t hum;
float temp, temperature;
int locsolo_state = LOW, on_off_command = LOW;
uint16_t  locsolo_duration;
uint16_t  locsolo_start;
short locsolo_flag = 0;
short locsolo_number = LOCSOLO_NUMBER - 1;
char device_name[] = "locsolo1";
int sleep_time_seconds = 900;                   //when watering is off, in seconds
int delay_time_seconds = 60;                   //when watering is on, in seconds
bool remote_update = 0;

void valve_turn_on();
void valve_turn_off();
int valve_state();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();
void web_update_setup();
void web_update();
void setup_wifi();

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
  if (valve_state) valve_turn_off();
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(mqtt_callback);
  setup_wifi();

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  if (!bmp.begin())  {
    Serial.println("BMP init failed!");
    bmp.setOversampling(16);
  }
  else Serial.println("BMP init success!");
}

void loop() {
  int r, i, len, http_code = 0;
  HTTPClient http;
  WiFiClient *stream;
  uint8_t buff[128] = { 0 };
  uint8_t mn = 0, count = 0;

  digitalWrite(ADC_SWITCH, 0);
  digitalWrite(VOLTAGE_TO_DIVIDER, 1);
  delay(200);
  voltage = 0;
  for (int j = 0; j < 50; j++) voltage += ESP.getVcc(); // voltage+=analogRead(A0); for new design
  digitalWrite(VOLTAGE_TO_DIVIDER, 0);
  voltage = (voltage / 50); //*5.7;                           //5.7 is the resistor divider value
  Serial.print("Voltage:");  Serial.println(voltage);

  digitalWrite(ADC_SWITCH, 1);
  delay(200);
  moisture = 0;
  for (int j = 0; j < 50; j++) moisture += analogRead(A0);
  moisture = (((float)moisture / 50) / 1024.0) * 100;
  Serial.print("Moisture:");  Serial.println(moisture);
  double t = 0, p = 0;
  delay(bmp.startMeasurment());
  bmp.getTemperatureAndPressure(T, P);
  for (int i = 0; i < 5; i++) {
    delay(bmp.startMeasurment());
    bmp.getTemperatureAndPressure(T, P);
    t += T;
    p += P;
  }
  T = t / 5;
  P = p / 5;
  Serial.print("T=");     Serial.print(T);
  Serial.print("   P=");  Serial.println(P);

  mqtt_reconnect();
  char buf_name[50];                                                    //berakni funkcioba szepen mint kell
  char buf[10];
  client.loop();
  dtostrf(T, 6, 1, buf);
  sprintf (buf_name, "%s%s", device_name, "/TEMPERATURE");
  client.publish(buf_name, buf);
  itoa((float)moisture, buf, 10);
  sprintf (buf_name, "%s%s", device_name, "/MOISTURE");
  client.publish(buf_name, buf);
  dtostrf((float)voltage / 1000, 6, 3, buf);
  sprintf (buf_name, "%s%s", device_name, "/VOLTAGE");
  client.publish(buf_name, buf);
  dtostrf(P, 6, 3, buf);
  sprintf (buf_name, "%s%s", device_name, "/PRESSURE");
  client.publish(buf_name, buf);
  sprintf (buf_name, "%s%s", device_name, "/READY_FOR_DATA");
  client.publish(buf_name, "0");
  client.loop();
  for (int i = 0; i < 20; i++) {
    delay(50);
    client.loop();
  }

  if (remote_update)  web_update();
  if (on_off_command && (float)voltage / 1000 > 3.0)  valve_turn_on();
  else  valve_turn_off();
  if (valve_state() == 0) {
    Serial.print("Valve state: "); Serial.println(valve_state());
    Serial.println("Deep Sleep");
    sprintf (buf_name, "%s%s", device_name, "/ON_OFF_STATE");
    client.publish(buf_name, "0");
    sprintf (buf_name, "%s%s", device_name, "/END");
    client.publish(buf_name, "0");
    delay(100);
    ESP.deepSleep(SLEEP_TIME, WAKE_RF_DEFAULT);
    delay(100);
  }
  else   {
    Serial.print("Valve state: "); Serial.println(valve_state());
    mqtt_reconnect();
    client.loop();
    sprintf (buf_name, "%s%s", device_name, "/ON_OFF_STATE");
    client.publish(buf_name, "1");
    sprintf (buf_name, "%s%s", device_name, "/END");
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

void valve_turn_on() {
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_TWO) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
  }
  if (valve_state) locsolo_state = HIGH;
}

void valve_turn_off() {
  uint16_t cnt = 0;
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_ONE) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
  }
  if (!valve_state) locsolo_state = LOW;
}

void battery_read() {
  Serial.print("Voltage: "); Serial.print((float)ESP.getVcc() / 1000); Serial.println("V");
  Serial.print("ADC function Voltage: "); Serial.print((float)analogRead(A0) * 5.7); Serial.println("V");
}

int valve_state() {
  return digitalRead(VALVE_SWITCH_TWO);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char buff[50];
  sprintf (buff, "%s%s", device_name, "/ON_OFF_COMMAND");
  if (!strcmp(topic, buff)) {
    on_off_command = payload[0] - 48;
    Serial.print("Valve command: ");  Serial.println(on_off_command);
  }
  sprintf (buff, "%s%s", device_name, "/DELAY_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    delay_time_seconds = atoi(buff);
    Serial.print("Delay time_seconds: "); Serial.println(delay_time_seconds);
  }
  sprintf (buff, "%s%s", device_name, "/SLEEP_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    sleep_time_seconds = atoi(buff);
    Serial.print("Sleep time_seconds: "); Serial.println(sleep_time_seconds);
  }
  sprintf (buff, "%s%s", device_name, "/REMOTE_UPDATE");
  if (!strcmp(topic, buff)) {
    remote_update = payload[0] - 48;
    Serial.print("Remote update: "); Serial.println(remote_update);
  }
}

void mqtt_reconnect() {
  char buf_name[50];
  if (!client.connected()) {
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    client.connect(clientId.c_str());
    sprintf (buf_name, "%s%s", device_name, "/ON_OFF_COMMAND");
    client.subscribe(buf_name);
    client.loop();
    sprintf (buf_name, "%s%s", device_name, "/SLEEP_TIME");
    client.subscribe(buf_name);
    client.loop();
    sprintf (buf_name, "%s%s", device_name, "/DELAY_TIME");
    client.subscribe(buf_name);
    client.loop();
    sprintf (buf_name, "%s%s", device_name, "/REMOTE_UPDATE");
    client.subscribe(buf_name);
    client.loop();
  }
}

void web_update_setup() {
  //MDNS.begin(host);
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      WiFiUDP::stopAll();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
  server.begin();
  //MDNS.addService("http", "tcp", 80);

  Serial.printf("Ready for update through browser! Open http://%s in your browser\n", host);
}

void web_update() {
  long int i = 0;
  web_update_setup();
  while (1)     {
    server.handleClient();
    delay(1);
    i++;
    if (i == WEB_UPDATE_TIMEOUT) {
      Serial.println("Timeout reached, restarting");
      ESP.restart();
    }
  }
}

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  wifiManager.setConnectTimeout(WIFI_CONNECTION_TIMEOUT);
  if (!wifiManager.autoConnect()) {
    Serial.println("Failed to connect and hit timeout. Entering deep sleep!");
    delay(50);
    ESP.deepSleep(0);
    delay(100);
  }
}
/*

  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);

  int i=0;
  while(i<600){
    i++;
    delay(100);
    if(i%10==0) Serial.print(".");
    if((WiFi.status()==WL_CONNECTED)) break;
  }
  if(!(WiFi.status()==WL_CONNECTED) && !wifiManager.startConfigPortal("ESP8266_client")){
    Serial.println("Not connected to WiFi");
    Serial.println("Deep Sleep");
    ESP.deepSleep(0);
  }
  }
*/

