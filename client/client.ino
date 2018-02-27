/*
   This program login into ESP8266_locsolo_server on ubuntu. Gets the A0 pin status from the server then set it. Also send Vcc voltage and temperature, etc.
   When A0 is HIGH: ESP8266 loggin happens every 30seconds, if it is LOW ---> deep sleep for x seconds
   by Norbi

   3V alatt ne nyisson ki a szelep, de ha nyitva van akkor legyen egy deltaU feszĂĽltsĂ©g ami alatt csukodk be (pl 2.9V)
*/
#include "client.h"
#include "certificates.h"
#include "communication.h"


DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server ( 80 );
BMP280 bmp;
WiFiClientSecure espClient;
PubSubClient client(espClient);
#if SZELEP
ADC_MODE(ADC_VCC); //only for old design
#endif

const char* host = "192.168.1.100";
const char* device_name = "szenzor1";
const char* device_passwd = "szenzor1";
int mqtt_port= 8883;
char device_id[127];
char mqtt_password[128];

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
uint32_t voltage;
double T, P;
float temp, temperature, moisture;
int RSSI_value;
int locsolo_state = LOW, on_off_command = LOW;
int sleep_time_seconds = 900;                   //when watering is off, in seconds
int delay_time_seconds = 60;                   //when watering is on, in seconds
bool remote_update = 0;
int flowmeter_int=0;
float flowmeter_volume, flowmeter_velocity;


void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println(ESP.getResetReason());
  void setup_pins();
  get_TempPressure();       //Azért az elején mert itt még nem melegedett fel a szenzor
  read_voltage();           //Ez azthiszem kitorolheto
  //pinMode(FLOWMETER, INPUT);
  //attachInterrupt(digitalPinToInterrupt(FLOWMETER), flow_meter_interrupt, FALLING);
  //digitalWrite(VOLTAGE_BOOST_EN_ADC_SWITCH, LOW);
  if (valve_state) valve_turn_off();
  Serial.println("Setting up certificates");
  espClient.setCertificate(certificates_esp8266_bin_crt, certificates_esp8266_bin_crt_len);
  espClient.setPrivateKey(certificates_esp8266_bin_key, certificates_esp8266_bin_key_len);
  Serial.println("Setting up mqtt callback, wifi");
  client.setServer(MQTT_SERVER, mqtt_port);
  client.setCallback(mqtt_callback);
  setup_wifi();

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  RSSI_value = WiFi.RSSI();
  Serial.println(RSSI_value);
  
}

void loop() {
  valve_test();
  read_voltage();
  read_moisture();
  mqtt_reconnect(); 
  Serial.println("mqtt_reconnect");
  Serial.println(client.state());  
  Serial.println(client.connected());
  if (client.connected()) {
    char buf_name[50];                                                    //berakni funkcioba szepen mint kell
    char buf[10];
    client.loop();
    mqttsend_d(T, device_id, "/TEMPERATURE", 1);
    mqttsend_d(moisture, device_id, "/MOISTURE", 2);
    mqttsend_i(RSSI_value, device_id, "/RSSI");
    mqttsend_d((float)voltage / 1000, device_id, "/VOLTAGE", 3);
    mqttsend_d(P, device_id, "/PRESSURE", 3);
    mqttsend_i(0, device_id, "/READY_FOR_DATA");
    client.loop();
    for (int i = 0; i < 20; i++) {    //Ez mire van? torolni ha nem kell
      delay(50);
      client.loop();
    }
  }

  Serial.println("Setting up webupdate if set");
  if (remote_update)  web_update();
  if (on_off_command && (float)voltage / 1000 > MINIMUM_VALVE_OPEN_VOLTAGE && !(client.state()))  {
    valve_turn_on();
  }
  else  valve_turn_off();
  if (valve_state() == 0) {
    if (client.connected()) {
      char buff_f[10];
      char buf_name[50];
      Serial.print("Valve state: "); Serial.println(valve_state());
      mqttsend_i(0, device_id, "/ON_OFF_STATE");
      mqttsend_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqttsend_i(0, device_id, "/END");
      delay(100);
      client.disconnect();
    }
    go_sleep(SLEEP_TIME);
    
  }
  else   {
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
  }
}

void valve_turn_on() {
#if SZELEP
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_TWO) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
  }
  if (valve_state) locsolo_state = HIGH;
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  Serial.println("Error turn on timeout reached");
#endif
}

void valve_turn_off() {
#if SZELEP  
  uint16_t cnt = 0;
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_ONE) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
  }
  if (!valve_state) locsolo_state = LOW;
  if((millis() - t) > MAX_VALVE_SWITCHING_TIME)  Serial.println("Error turn off timeout reached");
#endif
}

int valve_state() {
#if SZELEP  
  return digitalRead(VALVE_SWITCH_TWO);
#else
  return 0;
#endif
}

void valve_test(){
    while(1){
      valve_turn_off();
      valve_turn_on();
    }
}

void setup_pins(){
  pinMode(GPIO15, OUTPUT);
  pinMode(VALVE_H_BRIDGE_RIGHT_PIN, OUTPUT);
  pinMode(VALVE_H_BRIDGE_LEFT_PIN, OUTPUT);
  pinMode(RXD_VCC_PIN, OUTPUT);
  pinMode(VALVE_SWITCH_ONE, INPUT);
  pinMode(VALVE_SWITCH_TWO, INPUT);
  pinMode(RXD_VCC_PIN, OUTPUT);
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
    microseconds -= micros();
  }
  Serial.print("Entering in deep sleep for: "); Serial.print(int(microseconds/1000000)); Serial.println(" s");
  delay(200);
  ESP.deepSleep(abs(microseconds - 200000)+1); //az elozo sort vonom ki
  delay(100);
}

void flow_meter_interrupt(){
  flowmeter_int++;
  /*
  float flowmeter_speed;
  static int last_int_time = 0;
  static float last_volume = 0;
  flowmeter_volume = (float)flowmeter_int / FLOWMETER_CALIB_VOLUME;
  flowmeter_speed = (1000/(millis() - last_int_time) * 7.5);
  Serial.print("Flowmeter volume: "); Serial.println(flowmeter_volume);
  Serial.print("Flowmeter speed: "); Serial.println(flowmeter_speed);
  Serial.print("Flowmeter speed2: "); Serial.println((flowmeter_volume - last_volume) / ((millis() - last_int_time)/1000));
  last_int_time = millis();
  last_volume = flowmeter_volume;*/
}

void flow_meter_calculate_velocity(){
  static int last_int_time = 0, last_int = 0;;

  flowmeter_volume = (float)flowmeter_int / FLOWMETER_CALIB_VOLUME;
  flowmeter_velocity = (float)(flowmeter_int - last_int) / (((int)millis() - last_int_time)/1000);
  last_int_time = millis();
  last_int = flowmeter_int;

  Serial.print("Flowmeter volume: "); Serial.println(flowmeter_volume);
  Serial.print("Flowmeter velocity: "); Serial.println(flowmeter_velocity);
}

void read_voltage(){
  //digitalWrite(VOLTAGE_BOOST_EN_ADC_SWITCH, 0);
  digitalWrite(GPIO15, 0);        //FSA3157 digital switch
  digitalWrite(RXD_VCC_PIN, 1);
  voltage = 0;
  for (int j = 0; j < 50; j++) voltage+=analogRead(A0); // for new design
  voltage = (voltage / 50)*4.7272*1.039;                         //4.3 is the resistor divider value, 1.039 is empirical for ESP8266
  Serial.print("Voltage:");  Serial.println(voltage);
  digitalWrite(RXD_VCC_PIN, 0);
  }

void read_moisture(){  
#if !SZELEP
  digitalWrite(GPIO15, 1);          //FSA3157 digital switch
  delay(200);
  moisture = 0;
  for (int j = 0; j < 50; j++) moisture += analogRead(A0);
  moisture = (((float)moisture / 50) / 1024.0) * 100;
  Serial.print("Moisture:");  Serial.println(moisture);
  digitalWrite(GPIO15, 0);
#endif  
}

void get_TempPressure(){
  if (!bmp.begin(SDA,SCL))  {
    Serial.println("BMP init failed!");
    bmp.setOversampling(16);
  }
  else Serial.println("BMP init success!");
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

void mqttsend_d(double payload, char* device_id, char* topic, char precision){
  char buff_topic[50];
  char buff_payload[10];
  dtostrf(payload, 9, precision, buff_payload);
  sprintf (buff_topic, "%s%s", device_id, topic);
  client.publish(buff_topic, buff_payload);
}

void mqttsend_i(int payload, char* device_id, char* topic){
  char buff_topic[50];
  char buff_payload[10];
  itoa(payload, buff_payload, 10);
  sprintf (buff_topic, "%s%s", device_id, topic);
  client.publish(buff_topic, buff_payload);
}


