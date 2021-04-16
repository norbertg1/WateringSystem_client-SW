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

WiFiClient WifiClient;
File f;

//const char* host = "192.168.1.100";
char device_id[16];
int mqtt_port = 8883;

int locsolo_state = LOW, on_off_command = LOW, winter_state = LOW;
int sleep_time_seconds = 900;                  //when watering is off, in seconds
int delay_time_seconds = 60;                   //when watering is on, in seconds
int remote_update = 0, remote_log=0;
int valve_timeout=0;
String ver;
String ID ;
int mqtt_done=0;

WiFiClientSecure WifiSecureClient;
mqtt mqtt_client(WifiSecureClient);

flowmeter Flowmeter;
voltage Battery;
moisture Soil;
thermometer Thermometer;
humiditySensor HumiditySensor;
pressureSensor PressureSensor;

void flowmeter_callback(){
  Flowmeter.flowmeter_interrupt();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length){
  mqtt_client.callback_function(topic, payload, length);
}

void setup() {
  Serial.begin(115200);
  println_out("\n-----------------------ESP8266 alive-----------------------------------------\n");
  println_out("Reset reason for CPU0:");
  println_out(reset_reason(0));
  println_out("Reset reason for CPU1:");
  println_out(reset_reason(1));
  Serial.println((int)mqtt_client._client);
  //if( reset_reason(0) != "POWERON_RESET" && reset_reason(0) != "SW_RESET" && reset_reason(0) != "DEEPSLEEP_RESET") alternative_startup();  //Ez azért hogy ha hiba volna a programban akkor is újarindulás után OTAn frissíthető váljon a rendszer
  ID = String((uint32_t)(ESP.getEfuseMac() >> 32), HEX) + String((uint32_t)ESP.getEfuseMac(), HEX);  //String function doesnt know uint64_t
  ID.toCharArray(device_id, 16);
  format();
  f = create_file();
  RTC_validateCRC();
  setup_wifi();
  print_out("SDK: ");   println_out(ESP.getSdkVersion());
  print_out("Version: "); println_out(VERSION);
  print_out("ID: ");   println_out(ID);
  print_out("MAC: ");   println_out(WiFi.macAddress());
  setup_pins();
  Thermometer.read();
  HumiditySensor.read();
  PressureSensor.read();
  Battery.read_voltage();
  Soil.read_moisture();
  //if (valve_state() && !winter_state) valve_turn_off();    //EEPROMba vagy SPIFFbe tarolni, hogy epp winter state van-e? utanna ellenorizni!!!
  if (valve_state()) valve_turn_off();
  Wait_for_WiFi();
  print_out("IP address:  ");
  println_out(String(WiFi.localIP().toString()));
  print_out("RSSI: ");
  println_out(String(WiFi.RSSI()));
  config_time();
  WifiSecureClient.setCACert(root_CA_cert);
  WifiSecureClient.setPrivateKey(ESP_RSA_key);
  WifiSecureClient.setCertificate(ESP_CA_cert);
  mqtt_client.setServer(MQTT_SERVER, mqtt_port);
  mqtt_client.setCallback(mqtt_callback);
  Serial.print("WifiSecureClient: "); Serial.println(WifiSecureClient.remoteIP());
  if((float)Battery.voltage/1000 > MINIMUM_UPDATE_VOLTAGE) 
  {
    println_out("Checking for update!");
    t_httpUpdate_return ret = httpUpdate.update(WifiClient,MQTT_SERVER, 80, "/esp/update/update.php", VERSION);
    http_update_answer(ret);
  }
}

void loop() {
  //valve_test();
  on_off_command = 0;
  mqtt_client.reconnect();
  if (mqtt_client.connected()) {
    send_measurements_to_server();
    print_out("Waiting for commands:\n");
    mqtt_client.loop();
    mqtt_done=0;
    for (int i = 0; i < 100; i++) {
      delay(100);
      mqtt_client.loop();                  //Itt várok az adatra, talán szebben is lehetne
      if (mqtt_done == 5) break;
      //print_out(".");
    }
    print_out("\nNumber of received commands: "); println_out(String(mqtt_done));
  }
  //if (remote_update && (valve_state() == 0 || winter_state == 1))  {web_update(remote_update); println_out("\nSetting up Webupdate");}
  if (winter_state == 1)  winter_mode(); 
  if (on_off_command && ((float)Battery.voltage / 1000) > MINIMUM_VALVE_OPEN_VOLTAGE && !(mqtt_client.state()))  valve_turn_on();
  if (!on_off_command || ((float)Battery.voltage / 1000) < VALVE_CLOSE_VOLTAGE || (mqtt_client.state()))        valve_turn_off();
  delay(100);
  if ((valve_state() == 1) || (valve_state() == 14))  valve_is_open(); //ha a szelep nyitva van
  else                                                valve_is_closed();

    print_out("Time in awake state: "); print_out(String((float)millis()/1000)); println_out(" s");
    println_out("Delay");
    delay(DELAY_TIME);
    on_off_command = 0;
    detachInterrupt(FLOWMETER_PIN);
    digitalWrite(GPIO15, LOW);
    Thermometer.read();         //Nyitott szelepnél minden újracsatlakozásnál mérek hőmérsékletet és légnyomást
    digitalWrite(GPIO15, HIGH);
    attachInterrupt(FLOWMETER_PIN, flowmeter_callback, FALLING);
}

void valve_is_open(){       //ha a szelep nyitva van
    print_out("Valve state: "); println_out(String(valve_state()));
    mqtt_client.reconnect();
    if (mqtt_client.connected()) {
      print_out("szelep nyitva");
      mqtt_client.loop();
      mqtt_client.send_i(on_off_command, device_id, "/ON_OFF_STATE"); //ez elég fura, utánnajárni
      mqtt_client.send_d(Flowmeter.get_volume(), device_id, "/FLOWMETER_VOLUME", 2);
      mqtt_client.send_d(Flowmeter.get_velocity(), device_id, "/FLOWMETER_VELOCITY", 2);
      mqtt_client.send_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqtt_client.send_i(0, device_id, "/END");
      mqtt_client.loop();
      delay(100);
      mqtt_client.disconnect();
    }
  }

void valve_is_closed(){                                                   //ha a szelep nincs nyitva
    if (mqtt_client.connected()) {
      print_out("\nszelept nincs nyitva\n");
      mqtt_client.reconnect();
      mqtt_client.send_i(valve_state(), device_id, "/ON_OFF_STATE");
      mqtt_client.send_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
      mqtt_client.send_i(0, device_id, "/END");
      delay(100);
      mqtt_client.disconnect();
    }
    go_sleep(SLEEP_TIME, 0);
  }

void send_measurements_to_server(){
  mqtt_client.loop();
  println_out("sending temperature");
  mqtt_client.send_d(Thermometer.temperature, device_id, "/TEMPERATURE", 1);
  println_out("sending moisture");
  mqtt_client.send_d(Soil.moisture, device_id, "/MOISTURE", 2);
  println_out("sending RSSI");
  mqtt_client.send_i(WiFi.RSSI(), device_id, "/RSSI");
  println_out("sending voltage,etc..");
  mqtt_client.send_d((float)Battery.voltage / 1000, device_id, "/VOLTAGE", 3);
  mqtt_client.send_d(0, device_id, "/PRESSURE", 3);
  mqtt_client.send_s(ver.c_str(), device_id, "/VERSION");
  mqtt_client.send_s(reset_reason(0).c_str(), device_id, "/RST_REASON");
  mqtt_client.send_d(Flowmeter.get_volume(), device_id, "/FLOWMETER_VOLUME_X", 2);    //ez azert kell hogy pontos legyen a ki be kapcsolás
  mqtt_client.send_d((float)millis()/1000, device_id, "/AWAKE_TIME_X", 2);
  mqtt_client.send_i(0, device_id, "/READY_FOR_DATA");
}

void winter_mode(){
  valve_turn_on();
  if (mqtt_client.connected()) {
    mqtt_client.reconnect();
    print_out("Winter mode\n"); print_out("Valve state: "); println_out(String(valve_state()));
    mqtt_client.send_i(valve_state(), device_id, "/ON_OFF_STATE");
    mqtt_client.send_d((float)millis()/1000, device_id, "/AWAKE_TIME", 2);
    mqtt_client.send_i(0, device_id, "/END");
    delay(100);
    mqtt_client.disconnect();
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
  println_out("Closing Valve");
  uint16_t cnt = 0;
  digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 1);
  digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 0);
  uint32_t t = millis();
  while (!digitalRead(VALVE_SWITCH_ONE) && (millis() - t) < MAX_VALVE_SWITCHING_TIME) {
    delay(100);
    closing_flag=1;
  }
  if (closing_flag==1)  {
    mqtt_client.send_d(Flowmeter.get_volume(), device_id, "/FLOWMETER_VOLUME", 2);
    mqtt_client.send_d(Flowmeter.get_velocity, device_id, "/FLOWMETER_VELOCITY", 2);
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
  print_out("VALVE_SWITCH_ONE:"); println_out(String(digitalRead(VALVE_SWITCH_ONE)));
  print_out("VALVE_SWITCH_TWO:"); println_out(String(digitalRead(VALVE_SWITCH_TWO)));

  if(digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))                         {ret=1;}  //Ha nyitva van
  if(digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE) && winter_state == 1)    {ret=2;}
  if(!digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))                         {ret=0;}
  if(digitalRead(VALVE_SWITCH_TWO) && digitalRead(VALVE_SWITCH_ONE))                          {ret=12;}
  if(!digitalRead(VALVE_SWITCH_TWO) && !digitalRead(VALVE_SWITCH_ONE))                        {ret=13;}
  if (((float)Battery.voltage / 1000) <= VALVE_CLOSE_VOLTAGE) ret = 5;
  if (((float)Battery.voltage / 1000) <= MINIMUM_VALVE_OPEN_VOLTAGE) ret += 4;
  if(valve_timeout) {ret+=10;}
  print_out("Voltage: "); print_out(String(voltage));
  print_out(", valve_timeout flag: "); print_out(String(valve_timeout));
  print_out(", Valve state: "); println_out(String(ret));
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

void go_sleep_callback(/*WiFiManager *myWiFiManager*/void *){
  go_sleep(SLEEP_TIME_NO_WIFI, 0);
}

void go_sleep(float microseconds, int winter_state){
  if (!winter_state) {
    println_out("Winter_state.");
    valve_turn_off();
  }
  //WiFi.disconnect();  //nehezen akart ezzel visszacsatlakozni
  //if (remote_log)  send_log();
  print_out("time in awake state: "); print_out(String((float)millis()/1000)); println_out(" s");
  if(microseconds - micros() > MINIMUM_DEEP_SLEEP_TIME){  //korrekcio a bekapcsolva levo idore 
    microseconds = microseconds - micros();
  }

  time_t now = time(nullptr);
  if( WiFi.status() != WL_CONNECTED ){//"Power on"
    rtcData.epoch += (time_t)millis()/1000; //Az RTCben tárolt epoch értékehz hozzáadom a bekapolcst állapotban levő időhosszt
    now = rtcData.epoch;  //Az RTCben tárolt érték lesz a jelenlegi időpont
  }
  else rtcData.epoch = now;
  rtcData.epoch += (time_t)microseconds/1000000; //Az RTCbet tárolt epoch értékét megnövelem az deepsleep állapotban levő időhosszal
  println_out(ctime(&now));
  RTC_saveCRC();
  print_out("Previous unsuccessful wifi connection attempts: "); print_out((String)rtcData.attempts);
  print_out("Entering in deep sleep for: "); print_out(String((float)microseconds/1000000)); println_out(" s");
  WifiSecureClient.stop();
  close_file();
  delay(100);
  ESP.deepSleep(30*1000000);
  ESP.deepSleep(microseconds); //az elozo sort vonom ki
  delay(100);
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

void alternative_startup(){
  Serial.println("Alternative startup");
  SPIFFS.end();
  setup_wifi();
  Wait_for_WiFi();
  //t_httpUpdate_return ret = ESPhttpUpdate.update(MQTT_SERVER, 80, "/esp/update/esp8266.php", ver);
  //http_update_answer(ret);
}

void valve_open_on_switch(){
#if SZELEP
  print_out("valve_open_on_switch: "); Serial.print(rtcData.open_on_switch);
  if ((rtcData.open_on_switch == 1 && rtcData.valid == 1) && ((float)get_voltage() / 1000.0) > MINIMUM_VALVE_OPEN_VOLTAGE){
    print_out("valve openning on switch!!!");  pinMode(VALVE_H_BRIDGE_RIGHT_PIN, OUTPUT);
    pinMode(VALVE_H_BRIDGE_LEFT_PIN, OUTPUT);
    delay(100);
    digitalWrite(VALVE_H_BRIDGE_RIGHT_PIN, 0);
    digitalWrite(VALVE_H_BRIDGE_LEFT_PIN, 1);
    while(((float)get_voltage() / 1000.0) > MINIMUM_VALVE_OPEN_VOLTAGE){ //Letesztelni hogy ez mukodik e!! Alacsony feszultsegnel be kell csukodnia!
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

void RTC_validateCRC(){
  // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
  uint32_t crc = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
  rtcData.valid = false;
  if( crc == rtcData.crc32 ) {
	println_out("RTC CRC TRUE");
	rtcData.valid = true;
  }
  else println_out("RTC CRC FALSE");
}

void RTC_saveCRC(){
  print_out("Saving RTC memory\n");
  rtcData.crc32 = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
}

uint32_t calculateCRC32( const uint8_t *data, size_t length ) {
  uint32_t crc = 0xffffffff;
  while( length-- ) {
	uint8_t c = *data++;
	for( uint32_t i = 0x80; i > 0; i >>= 1 ) {
	  bool bit = crc & 0x80000000;
	  if( c & i ) {
		bit = !bit;
	  }

	  crc <<= 1;
	  if( bit ) {
		crc ^= 0x04c11db7;
	  }
	}
  }

  return crc;
}

String reset_reason(int icore) //returns reset reason for specified core
{
  switch (rtc_get_reset_reason( (RESET_REASON) icore))
  {
    case 1 : return (String)("POWERON_RESET");          /**<1,  Vbat power on reset*/
    case 3 : return (String)("SW_RESET");               /**<3,  Software reset digital core*/
    case 4 : return (String)("OWDT_RESET");             /**<4,  Legacy watch dog reset digital core*/
    case 5 : return (String)("DEEPSLEEP_RESET");        /**<5,  Deep Sleep reset digital core*/
    case 6 : return (String)("SDIO_RESET");             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : return (String)("TG0WDT_SYS_RESET");       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : return (String)("TG1WDT_SYS_RESET");       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : return (String)("RTCWDT_SYS_RESET");       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : return (String)("INTRUSION_RESET");       /**<10, Instrusion tested to reset CPU*/
    case 11 : return (String)("TGWDT_CPU_RESET");       /**<11, Time Group reset CPU*/
    case 12 : return (String)("SW_CPU_RESET");          /**<12, Software reset CPU*/
    case 13 : return (String)("RTCWDT_CPU_RESET");      /**<13, RTC Watch dog Reset CPU*/
    case 14 : return (String)("EXT_CPU_RESET");         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : return (String)("RTCWDT_BROWN_OUT_RESET");/**<15, Reset when the vdd voltage is not stable*/
    case 16 : return (String)("RTCWDT_RTC_RESET");      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : return (String)("NO_MEAN");
  }
}