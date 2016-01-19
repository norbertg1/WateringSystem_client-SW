/*
 *  This program login into ESP8266_locsolo_server. Gets the A0 pin status from the server then sets it. Also sends Vcc voltage and temperature.
 *  When A0 is HIGH ESP8266 loggin in to the serve every 30seconds, if it is LOW goind to deep sleep for 300seconds
 *  Created on 2015.08-20155.11
 *  by Norbi
 */

#include <ESP8266WiFi.h>
#include <DHT.h>

#define SLEEP_TIME 900000000    //in milliseconds
#define DHT_PIN 0
#define DHT_TYPE DHT11

const char* ssid     = "wifi";
const char* password = "";

const char* host = "192.168.1.100";

short locsolo=LOW;
short count=0;

ADC_MODE(ADC_VCC);
DHT dht(DHT_PIN,DHT_TYPE);

uint32_t voltage;
uint8_t hum;
float temp,temperature;

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(0, OUTPUT); //set as output
  digitalWrite(0, 0); //set to logical 0
  
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
  dht.begin();           // initialize temperature sensor
  }

void loop() {
  WiFiClient client;
  String locsolo_name = "locsolo3";                              //!!!!! ITT VALTOZTATNI A NEVEN !!!!!!
  const int httpPort = 80;
  voltage=0;
  temp=0;
  uint8_t mn=0;
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
  count = 0;
  int r,i;
while(1){                                 //ket rossz dolog tortenhet: az ESP szerverhez nem tudok csatlakozni vagy csatlakozni tudok de nem kapok 
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
      Serial.println(line);
      
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

