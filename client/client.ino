/*
   This program login into ESP8266_locsolo_server on ubuntu. Gets the A0 pin status from the server then set it. Also send Vcc voltage and temperature, etc.
   When A0 is HIGH: ESP8266 loggin happens every 30seconds, if it is LOW ---> deep sleep for x seconds
   by Norbi

   v2.4.1 verzióban be kell kapcsolni a Tools --> Debug port ---> Serial beállítást. Ez nélkül ha megszakad a wifi kapcsolat 
   az mqtt_reconnect() funkció csak 3-4 próbálkozás alatt tud sikeresen visszacsatlakozni, felette már nem. Ezt a az opciót bekapcsolva viszont bármennyi próbálkozás után sikeres lehet az újracsatlakozás

   v2.4.1 verzióban Tools --> lwIP Variant --> v1.4 Compile from source opció (többit nem teszteltem, kivéve: defaulnál (v2 Lower Memory) nem működik) szükséges a TLS MQTT titkosított kapcsolat működéséhez
*/
#include "client.h"
#include "certificates.h"
#include "communication.h"


//DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server (80);
BMP280 bmp;
WiFiClientSecure espClient;
PubSubClient client(espClient);
#if SZELEP
ADC_MODE(ADC_VCC);
#endif

//const char* host = "192.168.1.100";
const char* mosquitto_user = "titok";
const char* mosquitto_pass = "titok";
char device_id[25];
int mqtt_port= 8883;

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
uint32_t voltage;
double T, P;
float temp, temperature, moisture;
int RSSI_value;
int locsolo_state = LOW, on_off_command = LOW;
int sleep_time_seconds = 900;                  //when watering is off, in seconds
int delay_time_seconds = 60;                   //when watering is on, in seconds
int remote_update = 0;
int flowmeter_int=0;
float flowmeter_volume, flowmeter_velocity;
int valve_timeout=0;
String ver;
int mqtt_done=0;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.print("\nESP8266 alive\n");
  Serial.println(ESP.getResetReason());
  String ID = String(ESP.getChipId(), HEX) + "-" + String(ESP.getFlashChipId(), HEX);
  ID.toCharArray(device_id, 25); 
  get_TempPressure();       //Azért az elején mert itt még nem melegedett fel a szenzor
  setup_pins();
  read_voltage();           //Ez azthiszem kitorolheto
  if (valve_state) valve_turn_off();  
  Serial.println("Setting up certificates");
  espClient.setCertificate(certificates_esp8266_bin_crt, certificates_esp8266_bin_crt_len);
  espClient.setPrivateKey(certificates_esp8266_bin_key, certificates_esp8266_bin_key_len);
  Serial.println("Setting up mqtt callback, wifi");
  client.setServer(MQTT_SERVER, mqtt_port);
  client.setCallback(mqtt_callback);
  setup_wifi();
  Serial.println("IP address:  ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
  //while(1){delay(100); RSSI_value = WiFi.RSSI(); Serial.println(RSSI_value);}
  configTime(1 * 3600, 3600, "pool.ntp.org", "time.nist.gov");
  ver = VERSION;
  ver += '.';
  ver += SZELEP;
  Serial.println(ver);
  Serial.println("checking for update");
  t_httpUpdate_return ret = ESPhttpUpdate.update(MQTT_SERVER, 80, "/esp/update/esp8266.php", ver);
  http_update_answer(ret);  
  //delay(100);

}

void loop() {
  //valve_test();
  read_voltage();
  read_moisture();
  mqtt_reconnect();
  //flow_meter_calculate_velocity(); 
  if (client.connected()) {
    char buf_name[50];
    char buf[10];
    mqtt_done=0;
    client.loop();
    mqttsend_d(T, device_id, "/TEMPERATURE", 1);
    mqttsend_d(moisture, device_id, "/MOISTURE", 2);
    mqttsend_i(WiFi.RSSI(), device_id, "/RSSI");
    mqttsend_d((float)voltage / 1000, device_id, "/VOLTAGE", 3);
    mqttsend_d(P, device_id, "/PRESSURE", 3);
    mqttsend_s(ver.c_str(), device_id, "/VERSION");
    mqttsend_s(ESP.getResetReason().c_str(), device_id, "/RST_REASON");
    mqttsend_d((float)flowmeter_int / FLOWMETER_CALIB_VOLUME, device_id, "/FLOWMETER_VOLUME_X", 2);    //ez azert kell hogy pontos legyen a ki be kapcsolás
    mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME_X", 2);
    mqttsend_i(0, device_id, "/READY_FOR_DATA");
    //delay(3000);
    client.loop();
    for (int i = 0; i < 200; i++) {    //Ez mire van? torolni ha nem kell
      delay(100);
      client.loop();                  //Itt várok az adatra, talán szebben is lehetne
      if (mqtt_done == 4) break;
    }
  }

  Serial.println("Setting up webupdate if set");
  if (remote_update && valve_state() == 0)  web_update(remote_update);
  if (on_off_command && (float)voltage / 1000 > MINIMUM_VALVE_OPEN_VOLTAGE && !(client.state()))  {
    valve_turn_on();
  }
  else  valve_turn_off();
  if (valve_state() != 1) {       //ha a szelep nincs nyitva
    if (client.connected()) {
      char buff_f[10];
      char buf_name[50];
      Serial.print("Valve state: "); Serial.println(valve_state());
      mqttsend_i(0, device_id, "/ON_OFF_STATE");
      //mqttsend_d(flowmeter_volume, device_id, "/FLOWMETER_VOLUME", 2);      //ez törölhető csak figyelem van-e indokolatlan megszakítás
      //mqttsend_d(flowmeter_velocity, device_id, "/FLOWMETER_VELOCITY", 2);  //ez törölhető csak figyelem van-e indokolatlan megszakítás
      mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqttsend_i(0, device_id, "/END");
      delay(100);
      client.disconnect();
    }
    go_sleep(SLEEP_TIME);    
  }
  else   {                        //ha a szelep nyitva van
    Serial.print("Valve state: "); Serial.println(valve_state());
    flow_meter_calculate_velocity();
    mqtt_reconnect();
    if (client.connected()) {
      char buff_f[10];
      char buf_name[50];
      client.loop();
      mqttsend_i(1, device_id, "/ON_OFF_STATE");
      mqttsend_d(flowmeter_volume, device_id, "/FLOWMETER_VOLUME", 2);
      mqttsend_d(flowmeter_velocity, device_id, "/FLOWMETER_VELOCITY", 2);
      mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqttsend_i(0, device_id, "/END");
      client.loop();
      delay(100);
      client.disconnect();
    }
    Serial.print("time in awake state: "); Serial.print((float)millis()/1000); Serial.println(" s");
    Serial.println("delay");
    delay(DELAY_TIME);
    on_off_command = 0;
    detachInterrupt(FLOWMETER_PIN);
    digitalWrite(GPIO15, LOW);
    get_TempPressure();         //Nyitott szelepnél minden újracsatlakozásnál mérek hőmérsékletet és légnyomást
    digitalWrite(GPIO15, HIGH);
    attachInterrupt(FLOWMETER_PIN, flow_meter_interrupt, FALLING);
  }
}

void valve_turn_on() {
#if SZELEP
  digitalWrite(GPIO15, HIGH); //VOLTAGE BOOST
  Serial.println("Valve_turn_on()");
  pinMode(FLOWMETER_PIN, INPUT);
  attachInterrupt(FLOWMETER_PIN, flow_meter_interrupt, FALLING);
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_TWO) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
  }
  if (valve_state) locsolo_state = HIGH;
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  {Serial.println("Error turn on timeout reached");  valve_timeout=1;}
#endif
}

void valve_turn_off() {
#if SZELEP
  bool closing_flag=0;
  Serial.println("Valve_turn_off");
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
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  {Serial.println("Error turn off timeout reached");  valve_timeout=1;}
#endif
}

int valve_state() {
#if SZELEP
  int ret=0;
  if(digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))    {ret=1;}
  if(!digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))    {ret=0;}
  if(digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))     {ret=2;}
  if(!digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))   {ret=3;}
  if(valve_timeout) { ret+=10;}
  return ret;  
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
  go_sleep(SLEEP_TIME_NO_WIFI);
}

void go_sleep(long long int microseconds){
  valve_turn_off();
  //WiFi.disconnect();  //nehezen akart ezzel visszacsatlakozni
  espClient.stop();
  Serial.print("time in awake state: "); Serial.print((float)millis()/1000); Serial.println(" s");
  if(microseconds - micros() > MINIMUM_DEEP_SLEEP_TIME){  //korrekcio a bekapcsolva levo idore 
    microseconds = microseconds - micros();
  }

  Serial.print("Entering in deep sleep for: "); Serial.print((float)microseconds/1000000); Serial.println(" s");
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

  Serial.print("Flowmeter volume: "); Serial.print(flowmeter_volume); Serial.println(" L");
  Serial.print("Flowmeter velocity: "); Serial.print(flowmeter_velocity); Serial.println(" L/min");
}

void read_voltage(){
#if !SZELEP  
  digitalWrite(GPIO15, 0);        //FSA3157 digital switch
  digitalWrite(RXD_VCC_PIN, 1);
  delay(200);
  voltage = 0;
  for (int j = 0; j < 20; j++) voltage+=analogRead(A0); // for new design
  voltage = (voltage / 20)*4.7272*1.039;                         //4.7272 is the resistor divider value, 1.039 is empirical for ESP8266
  Serial.print("Voltage:");  Serial.println(voltage);
  digitalWrite(RXD_VCC_PIN, 0);
#else
  voltage = 0;
  for (int j = 0; j < 10; j++) {voltage+=ESP.getVcc(); /*Serial.println(ESP.getVcc());*/}
  voltage = (voltage / 10) - VOLTAGE_CALIB; //-0.2V
  Serial.print("Voltage:");  Serial.println(voltage);
#endif
  }

void read_moisture(){  
#if !SZELEP
  digitalWrite(GPIO15, 1);          //FSA3157 digital switch
  delay(1000);
  moisture = 0;
  for (int j = 0; j < 20; j++) moisture += analogRead(A0);
  moisture = (((float)moisture / 20) / 1024.0) * 100;
  Serial.print("Moisture:");  Serial.println(moisture);
  digitalWrite(GPIO15, 0);
  delay(100);
#endif  
}

void get_TempPressure(){
  
  if (!bmp.begin(SDA,SCL))  {
    Serial.println("BMP init failed!");
    //bmp.setOversampling(16);
  }
  else {Serial.println("BMP init success!"); bmp.setOversampling(16);}
  double t = 0, p = 0;
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
}

void http_update_answer(t_httpUpdate_return ret){
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("[update] Update failed.");
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[update] Update no Update.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("[update] Update ok."); // may not called we reboot the ESP
      break;    
  }
}

