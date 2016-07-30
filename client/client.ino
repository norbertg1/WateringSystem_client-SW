/*
 *  This program login into ESP8266_locsolo_server. Gets the A0 pin status from the server then sets it. Also sends Vcc voltage and temperature.
 *  When A0 is HIGH ESP8266 loggin in to the serve every 30seconds, if it is LOW goind to deep sleep for 300seconds
 *  Created on 2015.08-2015.11
 *  by Norbi
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>

#define SLEEP_TIME 60000000//900000000    //in microseconds
#define DHT_PIN 0
#define DHT_TYPE DHT11
#define LOCSOLO_NUMBER  1
#define LOCSOLO_PIN     5

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
short count=0;
short locsolo_number = LOCSOLO_NUMBER - 1;

void not_found_handle();

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(LOCSOLO_PIN, OUTPUT); //set as output
  digitalWrite(LOCSOLO_PIN, 0); //set to logical 0
  
  Serial.println();
  Serial.println();
  Serial.print("Connecting to wifi:");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  uint8_t count=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    count++;
    if(count>100) break;
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
//  server.on ( "/locsolo", locsolo_handle );
//  server.onNotFound ( not_found_handle );
//  server.begin();

  dht.begin();           // initialize temperature sensor
  }

void loop() {
  voltage=0;
  temp=0;
  int r,i,len,http_code;
  HTTPClient http;
  WiFiClient * stream;
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
  if(isnan(temp) || isnan(hum)) {temp=0;hum=0;/*Serial.println("Entering Deep Sleep mode"); delay(100); ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT); ki kell probalni*/  delay(100);}
  Serial.print("connecting to server: ");
  Serial.println(host);  // Use WiFiClient class to create TCP connections

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  IPAddress IP=WiFi.localIP();
  String s="http://192.168.1.100/client?=" + String(locsolo_number) + "&=" + String(temp) + "&=" + String(hum) + "&=" + String(voltage/50) + "&=" + IP[0] + "." + IP[1] + "." + IP[2] + "." + IP[3]; 
  Serial.println(s);
  http.begin(s);
  while(http_code!=200 && count < 3){
    http_code=http.GET();
    count++;
    if(http_code != 200)  {delay(5000); Serial.println("cannot connect, reconnecting...");}
  }
  len = http.getSize();
  stream = http.getStreamPtr();
  while(http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if(size)  {
      int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
      if(len > 0) len -= c;
    }
  }
  http.end();
  String buff_string;
  for(int k=0; k<20; k++){
      Serial.print((char)buff[k]);
      buff_string += String((char)buff[k]);
      }
  Serial.println(buff_string);
  locsolo_state=(buff_string.substring(6,(buff_string.indexOf("_")))).toInt();
//  locsolo_duration=(buff_string.substring(17,(buff_string.indexOf("_")))).toInt();
  Serial.println(locsolo_state);
  digitalWrite(LOCSOLO_PIN, locsolo_state);
  if(locsolo_state == 0){
    Serial.println("Deep Sleep");  
    ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);
    delay(100);
  }
  else    delay(10000); 
}
/*  Serial.println(locsolo_duration);
  if(locsolo_state == 1 && locsolo_duration>0 && locsolo_flag==0){
    locsolo_flag=1;
    locsolo_start=millis()/1000;
    digitalWrite(LOCSOLO_PIN, locsolo_state);                       //locsolo_ON
  }
  if((locsolo_flag==1 && ((millis()/1000)-locsolo_start) > locsolo_duration) || !locsolo_state){
    locsolo_flag==0;
    digitalWrite(LOCSOLO_PIN, locsolo_state);                       //locsolo_OFF
  }
  if(locsolo_flag==0) {
    Serial.println("Deep Sleep");
    ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);
    delay(100); //ha sikeres csatlakozas és informacio csere tortént, de nincs magas jelszint az A0 labon (nem kell ontozni) alvasba kuldom a klienst
  }
  delay(10000);
}

void not_found_handle(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}
/* 
  
  //ket rossz dolog tortenhet: az ESP szerverhez nem tudok csatlakozni vagy csatlakozni tudok de nem kapok 
    i=0;                                  //vagy rossz valaszt kapokt (pl. ha epp lefagy). Ha 3x rossz valaszt kapok alvasba kuldom a klienst,
    do{                                   //mikozben minden ilyen kapcsolodasnal 10x proballok kapcsolodni
      r=client.connect(host, httpPort);   //kapcsolodom az ESP szerverhez, 10x probalkozok
      delay(200);
      Serial.print("client.connect:"); Serial.println(r);
      Serial.println(i);
      i++;
    }while(!r && i<10);
    if (!r) {                                              //ha nem sikerult csatlakozni a ESP8266 szerver modulhoz
          Serial.println("connection failed");
          digitalWrite(0, 0);
          Serial.print("Voltage: ");
          Serial.println(voltage/50);
          Serial.print("Temperature: ");
          Serial.println(temp);
          Serial.print("Humidity: ");
          Serial.println(hum);
          Serial.println("Entering Deep Sleep mode");
          delay(100);
          ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);    //ha elerem a 10x csatalkozast es sikertelen alvasba kuldom a klienst
          delay(100);
          return;
        }
    client.println(locsolo_name);      // This will send the request to the server
    delay(10);
    
    // Read all the lines of the reply from server and print them to Serial
    while(client.available()){                                                          //ha sikerül csatlakozni
      String line = client.readStringUntil('\r');
      /*Serial.print("EPS8266 server replies:")*/
     /* Serial.println(line);
      
      if(line == (locsolo_name + "="))
      {
        String line = client.readStringUntil('\r');
        locsolo=line.toInt();
        digitalWrite(0, locsolo);  //Set A0 pin to locsolo
        client.println(locsolo+1);   //a kliens visszaküldi milyen állapotban van, +1 igy tudom érzékelni a hibás kapcsolatot
        client.println(voltage/50);   //a kliens visszaküldi mekkora az akkumulátor feszültsége
        client.println(temp);     //a kliens visszaküldi a hőmérsékletet
        client.println(hum);     //a kliens visszaküldi a hőmérsékletet
        Serial.print("ESP_"); Serial.print(locsolo_name); Serial.print(" reports "); Serial.print(locsolo_name); Serial.print(" status:");
        Serial.println(locsolo);
        goto ide;
       }else{
        count++; 
        Serial.print("No "); Serial.println(locsolo_name);
        delay(100);
        //goto ide;
      }
    }
    count++; 
    Serial.println("Infinite loop, see server");
    delay(10000);
    client.flush(); client.stop();
    if(count>3) {digitalWrite(0,0); locsolo= LOW; Serial.println("Authetification failed after 3 times"); break;}
  }
ide:  
  Serial.println("closing connection");
  if(locsolo == LOW)
  {
    Serial.print("Voltage: ");
    Serial.println(voltage/50);
    Serial.print("Temperature: ");
    Serial.println(temp);
    Serial.print("Humidity: ");
    Serial.println(hum);
    Serial.println("Entering Deep Sleep mode");
    delay(100);
    ESP.deepSleep(SLEEP_TIME,WAKE_RF_DEFAULT);
    delay(100); //ha sikeres csatlakozas és informacio csere tortént, de nincs magas jelszint az A0 labon (nem kell ontozni) alvasba kuldom a klienst
  }
  //delay(30000);    //ha magas jelszinten van az A0 kivezetes (kell ontozni) akkor nem kuldom alvasba a klienst csak alacsony fogyasztasu varakozo uezmmodba 30 masodpercig0                             
  delay(60000);    //2016.1.17 ha magas jelszinten van az A0 kivezetes (kell ontozni) akkor nem kuldom alvasba a klienst csak alacsony fogyasztasu varakozo uezmmodba 60 masodpercig0                             
  }
*/
