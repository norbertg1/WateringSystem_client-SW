/*
   This program login into ESP8266_locsolo_server on ubuntu. Gets the A0 pin status from the server then set it. Also send Vcc voltage and temperature, etc.
   When A0 is HIGH: ESP8266 loggin happens every 30seconds, if it is LOW ---> deep sleep for x seconds
   by Norbi

   v2.4.1 verzióban be kell kapcsolni a Tools --> Debug port ---> Serial beállítást. Ez nélkül ha megszakad a wifi kapcsolat 
   az mqtt_reconnect() funkció csak 3-4 próbálkozás alatt tud sikeresen visszacsatlakozni, felette már nem. Ezt a az opciót bekapcsolva viszont bármennyi próbálkozás után sikeres lehet az újracsatlakozás

   v2.4.1 verzióban Tools --> lwIP Variant --> v1.4 Compile from source opció (többit nem teszteltem, kivéve: defaulnál (v2 Lower Memory) nem működik) szükséges a TLS MQTT titkosított kapcsolat működéséhez

   Wifihez csatlakozás: Bekapcsolni a kapcsolóval, beírni a csatalkozási adatokat, majd ki be kapcsolni ismét a kapcsolóval.

   TODO:
   2019.3.31 //EEPROMba vagy SPIFFbe tarolni, hogy epp winter state van-e? utanna ellenorizni!!!
   
*/
#include "main.hpp"
#include "communication.hpp"
#include "certificates.h"
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
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>
#include "FS.h"
#include <time.h>

//DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server (80);
BMP280 bmp;
WiFiClientSecure espClient;
PubSubClient client(espClient);
File f;
#if SZELEP
ADC_MODE(ADC_VCC);
#endif

//const char* host = "192.168.1.100";
const char* mosquitto_user = "titok";
const char* mosquitto_pass = "titok";
char device_id[25];
int mqtt_port= 8883;

uint32_t voltage;
double T, P;
float temp, temperature, moisture;
int RSSI_value;
int locsolo_state = LOW, on_off_command = LOW, winter_state = LOW;
int sleep_time_seconds = 900;                  //when watering is off, in seconds
int delay_time_seconds = 60;                   //when watering is on, in seconds
int remote_update = 0, remote_log=0;
int flowmeter_int=0;
float flowmeter_volume, flowmeter_velocity;
int valve_timeout=0;
String ver;
String ID;
int mqtt_done=0;

void setup() {
  //valve_open_on_button(); nehézkessé teszi a soros port használatát ezért van kikommentelve
  if( ESP.getResetReason() != "Power on" && ESP.getResetReason() != "Deep-Sleep Wake" && ESP.getResetReason() != "Software/System restart") alternative_startup();  //Ez azért hogy ha hiba volna a programban akkor is újarindulás után OTAn frissíthető váljon a rendszer
  Serial.begin(115200);
  delay(50);
  ID = String(ESP.getChipId(), HEX) + "-" + String(ESP.getFlashChipId(), HEX);
  ID.toCharArray(device_id, 25); 
  format();
  f = create_file();
  println_out("\n-----------------------ESP8266 alive-----------------------------------------\n");
  println_out(ESP.getResetReason());
  rtc_read();
  //valve_open_on_switch();
  setup_wifi();
  print_out("ID: ");   println_out(ID);
  print_out("MAC: ");   println_out(WiFi.macAddress());
  get_TempPressure();       //Azért az elején mert itt még nem melegedett fel a szenzor
  setup_pins();
  read_voltage();
  read_moisture();
  //if (valve_state() && !winter_state) valve_turn_off();    //EEPROMba vagy SPIFFbe tarolni, hogy epp winter state van-e? utanna ellenorizni!!!
  if (valve_state()) valve_turn_off();
  println_out("Setting up certificates");
  espClient.setCertificate(certificates_esp8266_bin_crt, certificates_esp8266_bin_crt_len);
  espClient.setPrivateKey(certificates_esp8266_bin_key, certificates_esp8266_bin_key_len);
  println_out("Setting up mqtt callback, wifi");
  client.setServer(MQTT_SERVER, mqtt_port);
  client.setCallback(mqtt_callback);
  config_time();
  Wait_for_WiFi();
  print_out("IP address:  ");
  println_out(String(WiFi.localIP()));
  print_out("RSSI: ");
  println_out(String(WiFi.RSSI()));
  ver = VERSION;  ver += '.';  ver += SZELEP;
  print_out("version:"); println_out(ver);
  rtcData.open_on_switch = 0;                 //ez a két sor arra kell ha a kapcsolóval akaruk aktiválni az öntözést
  RTC_save();
  if((float)voltage/1000 > MINIMUM_UPDATE_VOLTAGE) 
  {
    println_out("checking for update!");
    t_httpUpdate_return ret = ESPhttpUpdate.update(MQTT_SERVER, 80, "/esp/update/esp8266.php", ver);
    http_update_answer(ret);
  }
}

void loop() {
  //valve_test();
  on_off_command = 0; //teszt 2018.9.13
  mqtt_reconnect();
  if (client.connected()) {
    mqtt_done=0;
    client.loop();
    println_out("sending temperature");
    mqttsend_d(T, device_id, "/TEMPERATURE", 1);
    println_out("sending moisture");
    mqttsend_d(moisture, device_id, "/MOISTURE", 2);
    println_out("sending RSSI");
    mqttsend_i(WiFi.RSSI(), device_id, "/RSSI");
    println_out("sending voltage,etc..");
    mqttsend_d((float)voltage / 1000, device_id, "/VOLTAGE", 3);
    mqttsend_d(P, device_id, "/PRESSURE", 3);
    mqttsend_s(ver.c_str(), device_id, "/VERSION");
    mqttsend_s(ESP.getResetReason().c_str(), device_id, "/RST_REASON");
    mqttsend_d((float)flowmeter_int / FLOWMETER_CALIB_VOLUME, device_id, "/FLOWMETER_VOLUME_X", 2);    //ez azert kell hogy pontos legyen a ki be kapcsolás
    mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME_X", 2);
    mqttsend_i(0, device_id, "/READY_FOR_DATA");
    print_out("Waiting for commands");
    client.loop();
    for (int i = 0; i < 1000; i++) {
      delay(10);
      client.loop();                  //Itt várok az adatra, talán szebben is lehetne
      if (mqtt_done == 5) break;
      print_out(".");
    }
  }
  if (remote_update && (valve_state() == 0 || winter_state == 1))  {web_update(remote_update); println_out("\nSetting up Webupdate");}
  if (winter_state == 1)  winter_mode(); 
  if (on_off_command && ((float)voltage / 1000) > MINIMUM_VALVE_OPEN_VOLTAGE && !(client.state()))  valve_turn_on();
  if (!on_off_command || ((float)voltage / 1000) < VALVE_CLOSE_VOLTAGE || (client.state()))        valve_turn_off();
  delay(100);
  if ((valve_state() == 1) || (valve_state() == 14)) {       //ha a szelep nyitva van
    print_out("Valve state: "); println_out(String(valve_state()));
    flow_meter_calculate_velocity();
    mqtt_reconnect();
    if (client.connected()) {
      print_out("szelept nyitva");
      client.loop();
      mqttsend_i(on_off_command, device_id, "/ON_OFF_STATE"); //ez elég fura, utánnajárni
      mqttsend_d(flowmeter_volume, device_id, "/FLOWMETER_VOLUME", 2);
      mqttsend_d(flowmeter_velocity, device_id, "/FLOWMETER_VELOCITY", 2);
      mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqttsend_i(0, device_id, "/END");
      client.loop();
      delay(100);
      client.disconnect();
    }
  }
  else{                                                   //ha a szelep nincs nyitva
    if (client.connected()) {
      print_out("\nszelept nincs nyitva\n");
      mqtt_reconnect();
      mqttsend_i(valve_state(), device_id, "/ON_OFF_STATE");
      mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqttsend_i(0, device_id, "/END");
      delay(100);
      client.disconnect();
    }
    go_sleep(SLEEP_TIME, 0);    
  }

    print_out("Time in awake state: "); print_out(String((float)millis()/1000)); println_out(" s");
    println_out("Delay");
    delay(DELAY_TIME);
    on_off_command = 0;
    detachInterrupt(FLOWMETER_PIN);
    digitalWrite(GPIO15, LOW);
    get_TempPressure();         //Nyitott szelepnél minden újracsatlakozásnál mérek hőmérsékletet és légnyomást
    digitalWrite(GPIO15, HIGH);
    attachInterrupt(FLOWMETER_PIN, flow_meter_interrupt, FALLING);
}

void winter_mode(){
  valve_turn_on();
  if (client.connected()) {
    mqtt_reconnect();
    print_out("Winter mode\n"); print_out("Valve state: "); println_out(String(valve_state()));
    mqttsend_i(valve_state(), device_id, "/ON_OFF_STATE");
    mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
    mqttsend_i(0, device_id, "/END");
    delay(100);
    client.disconnect();
    }
    go_sleep(SLEEP_TIME, 1);
}

void valve_turn_on() {
#if SZELEP
  digitalWrite(GPIO15, HIGH); //VOLTAGE BOOST
  println_out("Valve_turn_on()");
  pinMode(FLOWMETER_PIN, INPUT);
  attachInterrupt(FLOWMETER_PIN, flow_meter_interrupt, FALLING);
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_TWO) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
  }
  if (valve_state) locsolo_state = HIGH;
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  {println_out("Error turn on timeout reached");  valve_timeout=1;}
#endif
}

void valve_turn_off() {
#if SZELEP
  bool closing_flag=0;
  println_out("Valve_turn_off");
  uint16_t cnt = 0;
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_ONE) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
    closing_flag=1;
  }
  if (closing_flag==1)  {
    flow_meter_calculate_velocity();
    mqttsend_d(flowmeter_volume, device_id, "/FLOWMETER_VOLUME", 2);
    mqttsend_d(flowmeter_velocity, device_id, "/FLOWMETER_VELOCITY", 2);
  }
  if (!valve_state) locsolo_state = LOW;
  digitalWrite(GPIO15, LOW);  //VOLTAGE_BOOST
  detachInterrupt(FLOWMETER_PIN); 
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  {println_out("Error turn off timeout reached");  valve_timeout=1;}
#endif
}

int valve_state() {
#if SZELEP
  int ret=0;
  println_out("\nVALVE_SWITCH_ONE:"); println_out(String(digitalRead(VALVE_SWITCH_ONE)));
  println_out("VALVE_SWITCH_TWO:"); println_out(String(digitalRead(VALVE_SWITCH_TWO)));

  if(digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))                         {ret=1;}  //Ha nyitva van
  if(digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE) && winter_state == 1)    {ret=2;}
  if(!digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))                         {ret=0;}
  if(digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))                          {ret=12;}
  if(!digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))                        {ret=13;}
  if (((float)voltage / 1000) <= VALVE_CLOSE_VOLTAGE) ret = 5;
  if (((float)voltage / 1000) <= MINIMUM_VALVE_OPEN_VOLTAGE) ret += 4;
  if(valve_timeout) {ret+=10;}
  print_out("voltage: "); print_out(String(voltage));
  print_out(" valve_timeout flag: "); print_out(String(valve_timeout));
  print_out(" Valve state: "); print_out(String(ret));
  print_out("\n");
  return ret;  //Nyitott állapot lehet ret = 1 vagy ret = 14
  //return digitalRead(VALVE_SWITCH_TWO);
#else
  return 0;
#endif
}

void valve_test(){
  while(1){
    valve_turn_off();
    valve_turn_on();
    delay(100);
    }
}

void setup_pins(){
  pinMode(GPIO15, OUTPUT);
  pinMode(VALVE_H_BRIDGE_RIGHT_PIN, OUTPUT);
  pinMode(VALVE_H_BRIDGE_LEFT_PIN, OUTPUT);
  pinMode(RXD_VCC_PIN, OUTPUT);
  pinMode(VALVE_SWITCH_ONE, INPUT);
  pinMode(VALVE_SWITCH_TWO, INPUT);
}

void go_sleep_callback(WiFiManager *myWiFiManager){
  go_sleep(SLEEP_TIME_NO_WIFI, 0);
}

void go_sleep(float microseconds, int winter_state){
  if (!winter_state) {
    print_out("winter_state\n ");
    valve_turn_off();
  }
  //WiFi.disconnect();  //nehezen akart ezzel visszacsatlakozni
  if (remote_log)  send_log();
  espClient.stop();
  print_out("time in awake state: "); print_out(String((float)millis()/1000)); println_out(" s");
  if(microseconds - micros() > MINIMUM_DEEP_SLEEP_TIME){  //korrekcio a bekapcsolva levo idore 
    microseconds = microseconds - micros();
  }
 
  time_t now = time(nullptr);
  if(now == 0 && rtcData.valid && ESP.getResetReason() != "Power on"){
    rtcData.epoch += (time_t)millis()/1000; //Az RTCben tárolt epoch értékehz hozzáadom a bekapolcst állapotban levő időhosszt
    now = rtcData.epoch;  //Az RTCben tárolt érték lesz a jelenlegi időpont
  }
  else rtcData.epoch = now;
  rtcData.epoch += (time_t)microseconds/1000000; //Az RTCbet tárolt epoch értékét megnövelem az deepsleep állapotban levő időhosszal
  println_out(ctime(&now));
  RTC_save();
  print_out("Previous unsuccessful wifi connection attempts: "); print_out((String)rtcData.attempts);
  print_out("Entering in deep sleep for: "); print_out(String((float)microseconds/1000000)); println_out(" s");
  close_file();
  delay(100);
  ESP.deepSleep(microseconds); //az elozo sort vonom ki
  delay(100);
}

void flow_meter_interrupt(){
  flowmeter_int++;
}

void flow_meter_calculate_velocity(){
  static int last_int_time = 0, last_int = 0;;

  flowmeter_volume = (float)flowmeter_int / FLOWMETER_CALIB_VOLUME;
  flowmeter_velocity = ((float)(flowmeter_int - last_int) / (((int)millis() - last_int_time)/1000))/FLOWMETER_CALIB_VELOCITY;
  //if (flowmeter_volume == 0) flowmeter_velocity=0;
  last_int_time = millis();
  last_int = flowmeter_int;

  print_out("Flowmeter volume: "); print_out(String(flowmeter_volume)); println_out(" L");
  print_out("Flowmeter velocity: "); print_out(String(flowmeter_velocity)); println_out(" L/min");
}

float read_voltage(){
#if !SZELEP  
  digitalWrite(GPIO15, 0);        //FSA3157 digital switch
  digitalWrite(RXD_VCC_PIN, 1);
  delay(100); //2018.aug.25
  voltage = 0;
  for (int j = 0; j < 20; j++) voltage+=analogRead(A0); // for new design
  //voltage = (voltage / 20)*4.7272*1.039;                         //4.7272 is the resistor divider value, 1.039 is empirical for ESP8266
  voltage = (voltage / 20)*4.7272*0.957;                                 //82039a-1640ef, 2018.aug.15
  print_out("Voltage:");  println_out(String(voltage));
  digitalWrite(RXD_VCC_PIN, 0);
  return voltage;
#else
  voltage = 0;
  for (int j = 0; j < 10; j++) {voltage+=ESP.getVcc(); /*Serial.println(ESP.getVcc());*/}
  voltage = (voltage / 10) - VOLTAGE_CALIB; //-0.2V
  print_out("Voltage:");  println_out(String(voltage));
  return voltage;
#endif
  }

void read_moisture(){  
#if !SZELEP
  digitalWrite(GPIO15, 1);          //FSA3157 digital switch
  delay(100);
  moisture = 0;
  for (int j = 0; j < 20; j++) moisture += analogRead(A0);
  moisture = (((float)moisture / 20) / 1024.0) * 100;
  print_out("Moisture:");  println_out(String(moisture));
  digitalWrite(GPIO15, 0);
  delay(10);
#endif  
}

void get_TempPressure(){
  
  if (!bmp.begin(SDA,SCL))  {
    println_out("BMP init failed!");
    bmp.setOversampling(16);
  }
  else {println_out("BMP init success!"); bmp.setOversampling(16);}
  double t = 0, p = 0;
  for (int i = 0; i < 3; i++) {
    delay(bmp.startMeasurment());
    bmp.getTemperatureAndPressure(T, P);
    t += T;
    p += P;
  }
  T = t / 3;
  P = p / 3;
  print_out("T=");     print_out(String(T));
  print_out("   P=");  println_out(String(P));
}

void http_update_answer(t_httpUpdate_return ret){
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      println_out("[update] Update failed.");
      break;
    case HTTP_UPDATE_NO_UPDATES:
      println_out("[update] Update no Update.");
      break;
    case HTTP_UPDATE_OK:
      println_out("[update] Update ok."); // may not called we reboot the ESP
      break;    
  }
}

File create_file(){
#if FILE_SYSTEM  
  SPIFFS.begin();
  char buff[13];
  sprintf (buff, "%s%s", "/", device_id);
  File f = SPIFFS.open(buff, "a+");
  if (!f) {
    Serial.println("log file open failed");
    SPIFFS.remove(buff);
    format_now();
  }
  if(f.size() > MAX_LOG_FILE_SIZE){      //Ha tul nagy
    f.close();
    SPIFFS.remove(buff);
    File f = SPIFFS.open(buff, "a+");
  }
  f.print("\nfile size:");
  f.print(f.size());
  Serial.print("\nfile size:");
  Serial.print(f.size());
  return f;
#endif
}

void close_file(){
#if FILE_SYSTEM
  f.close();
#endif
}

void print_out(String str){
#if SERIAL_PORT  
  Serial.print(str);
#endif
#if FILE_SYSTEM  
  f.print(str);
#endif
}

void println_out(String str){
#if SERIAL_PORT
  Serial.println(str);
  Serial.print((float)millis()/1000); Serial.print(":");
#endif
#if FILE_SYSTEM
  f.println(str);
  f.print((float)millis()/1000); f.print(":");
#endif
}

void config_time(){
#ifdef CONFIG_TIME
  configTime(2 * 3600, 3600, "pool.ntp.org", "time.nist.gov", "time.google.com");
  println_out("Get current time request");
#endif
}

void format(){
#if FILE_SYSTEM
  SPIFFS.begin();
  if (!SPIFFS.exists("/formok")) {
    Serial.println("Please wait 30 secs for SPIFFS to be formatted");
    if(SPIFFS.format()) Serial.println("Spiffs formatted");
    File file = SPIFFS.open("/formok", "w");
    if (!file) {
        Serial.println("file open failed");
    } else {
        file.println("Format Complete");
        file.close();
        Serial.println("format file written and closed");
    }
  } else {
    println_out("SPIFFS is formatted. Moving along...");
  }
#endif
}

void format_now(){
#if FILE_SYSTEM
  if(SPIFFS.format()) Serial.println("SPIFFS formatted");
  else Serial.println("SPIFFS format failed");
#endif
}

void alternative_startup(){
  SPIFFS.end();
  setup_wifi();
  Wait_for_WiFi();
  t_httpUpdate_return ret = ESPhttpUpdate.update(MQTT_SERVER, 80, "/esp/update/esp8266.php", ver);
  http_update_answer(ret);
}

void RTC_save(){
  print_out("Saving RTC memory\n");
  rtcData.crc32 = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
  ESP.rtcUserMemoryWrite( 0, (uint32_t*)&rtcData, sizeof( rtcData ) );
}

void valve_open_on_button(){
#if SZELEP
  if(ESP.getChipId() != 0x795041 && ESP.getChipId() != 0x288f83){
    pinMode(TXD_PIN, INPUT);
    if(!digitalRead(TXD_PIN) && read_voltage() > MINIMUM_VALVE_OPEN_VOLTAGE){
      pinMode(VALVE_H_BRIDGE_RIGHT_PIN, OUTPUT);
      pinMode(VALVE_H_BRIDGE_LEFT_PIN, OUTPUT);
      delay(100);
      digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
      digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
      wifi_fpm_close();
      WiFi.forceSleepBegin();
      while(read_voltage() > MINIMUM_VALVE_OPEN_VOLTAGE){
        delay(60000);
      }
      digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
      digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
      delay(MAX_VALVE_SWITCHING_TIME);
      ESP.deepSleep(sleep_time_seconds);
    }
    pinMode(TXD_PIN, OUTPUT);
  }
#endif 
}

void valve_open_on_switch(){
#if SZELEP
  print_out("valve_open_on_switch: "); Serial.print(rtcData.open_on_switch);
  if ((rtcData.open_on_switch == 1 && rtcData.valid == 1) && ((float)read_voltage() / 1000.0) > MINIMUM_VALVE_OPEN_VOLTAGE){
    print_out("valve openning on switch!!!");  pinMode(VALVE_H_BRIDGE_RIGHT_PIN, OUTPUT);
    pinMode(VALVE_H_BRIDGE_LEFT_PIN, OUTPUT);
    delay(100);
    digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
    digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
    wifi_fpm_close();
    WiFi.forceSleepBegin();
    while(((float)read_voltage() / 1000.0) > MINIMUM_VALVE_OPEN_VOLTAGE){ //Letesztelni hogy ez mukodik e!! Alacsony feszultsegnel be kell csukodnia!
      delay(60000);
    }
    digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
    digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
    delay(MAX_VALVE_SWITCHING_TIME);
    rtcData.open_on_switch = 0;
    RTC_save();
    ESP.deepSleep(sleep_time_seconds);
  }
  rtcData.open_on_switch = 1;
  RTC_save();
#endif 
}