 /*
 *  This program login into ESP8266_locsolo_server. Gets the A0 pin status from the server then sets it. Also sends Vcc voltage and temperature.
 *  When A0 is HIGH ESP8266 loggin in to the serve every 30seconds, if it is LOW goind to deep sleep for 300seconds
 *  Created on 2015.08-2015.11
 *  by Norbi
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include <DHT.h>

#define SLEEP_TIME_SECONDS          60  //900                             //when watering is off, in seconds
#define DELAY_TIME_SECONDS          30                                    //when watering is on, in seconds
#define SLEEP_TIME_NO_WIFI_SECONDS  60                                    //when cannot connect to saved wireless network in seconds, this is the time until we can set new SSID

#define SLEEP_TIME_NO_WIFI          SLEEP_TIME_NO_WIFI_SECONDS * 1000000  //when cannot connect to saved wireless network, this is the time until we can set new wifi SSID
#define SLEEP_TIME                  SLEEP_TIME_SECONDS * 1000000          //when watering is off, in microseconds
#define DELAY_TIME                  DELAY_TIME_SECONDS * 1000             //when watering is on, in miliseconds
#define DHT_PIN                     0
#define DHT_TYPE                    DHT11
#define LOCSOLO_NUMBER              1
#define LOCSOLO_PIN                 5

const char* ssid     = "wifi";
const char* password = "";

const char* host = "192.168.1.100";

ADC_MODE(ADC_VCC);
DHT dht(DHT_PIN,DHT_TYPE);
ESP8266WebServer server ( 80 );

uint32_t voltage;
uint8_t hum;
float temp,temperature;
short locsolo_state=LOW;
uint16_t  locsolo_duration;
uint16_t  locsolo_start;
short locsolo_flag=0;
short locsolo_number = LOCSOLO_NUMBER - 1;

void not_found_handle();

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(LOCSOLO_PIN, OUTPUT); //set as output
  digitalWrite(LOCSOLO_PIN, 0); //set to logical 0
  
  Serial.println();
  WiFiManager wifiManager;
  wifiManager.setTimeout(120);
  if(!wifiManager.autoConnect("Watering_client1")) {
    Serial.println("failed to connect and hit timeout");
    ESP.deepSleep(SLEEP_TIME_NO_WIFI,WAKE_RF_DEFAULT);
    delay(100);
  }

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  MDNS.begin("watering_client1");
  MDNS.addService("watering_server", "tcp", 8080);
  dht.begin();           // initialize temperature sensor
  
  }

void loop() {
  voltage=0;
  temp=0;
  int r,i,len,http_code=0;
  HTTPClient http;
  WiFiClient *stream;
  uint8_t buff[128] = { 0 };
  uint8_t mn=0,count=0;
  
  for(int j=0;j<50;j++)
  {
    voltage+=ESP.getVcc();
  }
  Serial.print("Voltage:");  Serial.print(voltage); 
    do{
      temp=dht.readTemperature();
      delay(500);
      mn++;
    }while(isnan(temp) && mn<5);
  mn=0;  
  Serial.print(" Temperature:"); Serial.println(temp);
  do{
      hum=dht.readHumidity();
      delay(500);
      mn++;
    }while(isnan(hum) && mn<5);
  if(isnan(temp) || isnan(hum)) {temp=0;hum=0; delay(100);}

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
  s+="/client?=" + String(locsolo_number) + "&=" + String(temp) + "&=" + String(hum) + "&=" + String(voltage/50) + "&=" + IP[0] + "." + IP[1] + "." + IP[2] + "." + IP[3]; 
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
  digitalWrite(LOCSOLO_PIN, locsolo_state);
  if(locsolo_state == 0){
    Serial.println("Deep Sleep");  
    http.end();
    delay(100);
    ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);
    delay(100);
  }
  else   {
    Serial.println("delay");
    delay(DELAY_TIME); 
  }
}
