/*
 
   Wifihez csatlakozás: Bekapcsolni a kapcsolóval, beírni a csatalkozási adatokat, majd ki be kapcsolni ismét a kapcsolóval.

*/
#include "main.hpp"

WiFiClient WifiClient;
File f;

//const char* host = "192.168.1.100";
char device_id[16];
int mqtt_port = 8883;
int sleep_time_seconds = 900;                  //when watering is off, in seconds
int delay_time_seconds = 60;                   //when watering is on, in seconds
int remote_update = 0, remote_log=0;

String ver;
String ID ;

WiFiClientSecure WifiSecureClient;
mqtt mqtt_client(WifiSecureClient);

valve_control Valve;
flowmeter Flowmeter;
voltage Battery;
moisture Soil;
thermometer Thermometer;
humiditySensor HumiditySensor;
pressureSensor PressureSensor;

void setup() {
  Serial.begin(115200);
  println_out("\n-----------------------ESP8266 alive-----------------------------------------\n");
  println_out("Reset reason for CPU0:");
  println_out(reset_reason(0));
  println_out("Reset reason for CPU1:");
  println_out(reset_reason(1));
  if( reset_reason(0) != "POWERON_RESET" && reset_reason(0) != "SW_RESET" && reset_reason(0) != "DEEPSLEEP_RESET") alternative_startup();  //Ez azért hogy ha hiba volna a programban akkor is újarindulás után OTAn frissíthető váljon a rendszer
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
  //if (Valve.state() && !Valve.winter_state) valve_turn_off();    //EEPROMba vagy SPIFFbe tarolni, hogy epp winter state van-e? utanna ellenorizni!!!
  if (Valve.state()) Valve.turn_off();
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
  //Valve.test();
  Valve.on_off_command = 0;
  mqtt_client.reconnect();
  if (mqtt_client.connected()) {
    send_measurements_to_server();
    mqtt_client.waiting_for_messages(5); //number off messages we waiting for
  }
  if (remote_update && (Valve.state() == 0 || Valve.winter_state == 1))  {webupdate(5); println_out("\nSetting up Webupdate");}
  if (Valve.winter_state == 1)  Valve.winter_mode(); 
  if (Valve.on_off_command && ((float)Battery.voltage / 1000) > MINIMUM_VALVE_OPEN_VOLTAGE && !(mqtt_client.state()))  Valve.turn_on();
  if (!Valve.on_off_command || ((float)Battery.voltage / 1000) < VALVE_CLOSE_VOLTAGE || (mqtt_client.state()))        Valve.turn_off();
  delay(100);
  if ((Valve.state() == 1) || (Valve.state() == 14))  Valve.is_open(); //ha a szelep nyitva van
  else                                                Valve.is_closed();

  print_out("Time in awake state: "); print_out(String((float)millis()/1000)); println_out(" s");
  println_out("Delay");
  delay(DELAY_TIME);
  Valve.on_off_command = 0;
  detachInterrupt(FLOWMETER_PIN);
  digitalWrite(GPIO15, LOW);
  Thermometer.read();         //Nyitott szelepnél minden újracsatlakozásnál mérek hőmérsékletet és légnyomást
  digitalWrite(GPIO15, HIGH);
  attachInterrupt(FLOWMETER_PIN, flowmeter_callback, FALLING);
}

void flowmeter_callback(){
  Flowmeter.flowmeter_callback();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length){
  mqtt_client.callback_function(topic, payload, length);
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
  if (!Valve.winter_state) {
    println_out("Winter_state.");
    Valve.turn_off();
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
  t_httpUpdate_return ret = httpUpdate.update(WifiClient,MQTT_SERVER, 80, "/esp/update/update.php", VERSION);
  http_update_answer(ret);
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