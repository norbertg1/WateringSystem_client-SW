#include "communication.hpp"
#include "main.hpp"

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
RTC_DATA_ATTR struct RTCData rtcData;
const char mqtt_user[] = "titok";
const char mqtt_pass[] = "titok";


mqtt::mqtt(WiFiClientSecure& cclient) : PubSubClient (cclient)
{
  pubsubclient = PubSubClient (cclient); 
}

void mqtt::callback_function(char* topic, byte* payload, unsigned int length) {     //ezekre a csatornakra iratkozok fel
  char buff[70];
  print_out("\nReceived message in topic: ");   
  println_out(topic);
  sprintf (buff, "%s%s", device_id, "/ON_OFF_COMMAND");
  if (!strcmp(topic, buff)) {
	Valve.on_off_command = payload[0] - 48;
	rtcData.winter_state = 0;
	if (Valve.on_off_command == 2){
	  print_out("Winter State");
	  Valve.winter_state = 1;
	  rtcData.winter_state = 1;
	} //Winter state, sajnos nem működik újraindulásnál hardveresen mindig becsukodik
	print_out("Valve command: ");  println_out(String(Valve.on_off_command));
	received_messages++;
  }
  sprintf (buff, "%s%s", device_id, "/DELAY_TIME");
  if (!strcmp(topic, buff)) {
	for (uint i = 0; i < length; i++) buff[i] = (char)payload[i];
	buff[length] = '\n';
	delay_time_seconds = atoi(buff);
	print_out("Delay time_seconds: "); println_out(String(delay_time_seconds));
	received_messages++;
  }
  sprintf (buff, "%s%s", device_id, "/SLEEP_TIME");
  if (!strcmp(topic, buff)) {
	for (uint i = 0; i < length; i++) buff[i] = (char)payload[i];
	buff[length] = '\n';
	sleep_time_seconds = atoi(buff);
	print_out("Sleep time_seconds: "); println_out(String(sleep_time_seconds));
	received_messages++;
  }
  sprintf (buff, "%s%s", device_id, "/REMOTE_UPDATE");
  if (!strcmp(topic, buff)) {
	remote_update = payload[0] - 48;
	print_out("Remote update: "); println_out(String(remote_update));
	received_messages++;
  }
  sprintf (buff, "%s%s", device_id, "/REMOTE_LOG");
  if (!strcmp(topic, buff)) {
	remote_log = payload[0] - 48;
	print_out("Remote log: "); println_out(String(remote_log));
	received_messages++;
  }
}

void mqtt::reconnect() {
	char buf_name[50];
	int i=0;
	int attempts_max = 3;
	print_out("Connceting to MQTT server as: "); println_out(ID);
	Serial.print("(int)mqtt_client._client: ");
	Serial.println((int)mqtt_client._client);
	mqtt_client.connect(ID.c_str(), mqtt_user , mqtt_pass);
	if (Valve.state()) attempts_max=20;
	Serial.println(mqtt_client.state());
	while(mqtt_client.state() != 0 && i < attempts_max){
		if (i>10) {           //nem tudom miert, talan bugos de ez kell ha nem akar csatlakozni
	  	//espClient.setCertificate(certificates_bin_crt);
	  	//espClient.setPrivateKey(certificates_bin_key);
		}
		print_out(".");
		i++;
  	}
	if(mqtt_client.connected()) {
	  mqtt_client.connect(ID.c_str(), mqtt_user , mqtt_pass);       //Az Ubuntun futó mosquitoba a bejelentkezési adatok
	  sprintf (buf_name, "%s%s", device_id, "/ON_OFF_COMMAND");
	  mqtt_client.subscribe(buf_name);
	  mqtt_client.loop();
	  sprintf (buf_name, "%s%s", device_id, "/SLEEP_TIME");
	  mqtt_client.subscribe(buf_name);
	  mqtt_client.loop();
	  sprintf (buf_name, "%s%s", device_id, "/DELAY_TIME");
	  mqtt_client.subscribe(buf_name);
	  mqtt_client.loop();
	  sprintf (buf_name, "%s%s", device_id, "/REMOTE_UPDATE");
	  mqtt_client.subscribe(buf_name);
	  sprintf (buf_name, "%s%s", device_id, "/REMOTE_LOG");
	  mqtt_client.subscribe(buf_name);
	  mqtt_client.send_i(i, device_id, "/DEBUG");
	  mqtt_client.loop();
	}
	print_out("\nMQTT attempts = "); print_out(String(i));
	print_out(" The nMQTT state is: "); println_out(String(mqtt_client.state()));
}

void mqtt::waiting_for_messages(int number){
	print_out("Waiting for messages:\n");
    mqtt_client.received_messages=0;
    for (int i = 0; i < 100; i++) {
      mqtt_client.loop();                  //Itt várok az adatra, talán szebben is lehetne
      if (mqtt_client.received_messages == number) break;
      //print_out(".");
		delay(100);
    }
    print_out("\nNumber of received messages: "); println_out(String(mqtt_client.received_messages));
}

void mqtt::send_d(double payload, char* device_id, char* topic, char precision){
  char buff_topic[50];
  char buff_payload[10];
  dtostrf(payload, 9, precision, buff_payload);
  sprintf (buff_topic, "%s%s", device_id, topic);
  mqtt_client.publish(buff_topic, buff_payload);
}

void mqtt::send_i(int payload, char* device_id, char* topic){
  char buff_topic[50];
  char buff_payload[10];
  itoa(payload, buff_payload, 10);
  sprintf (buff_topic, "%s%s", device_id, topic);
  mqtt_client.publish(buff_topic, buff_payload);
}

void mqtt::send_s(const char *payload, char* device_id, char* topic){
  char buff_topic[50];
  sprintf (buff_topic, "%s%s", device_id, topic);
  mqtt_client.publish(buff_topic, payload);
}

void setup_wifi() {
  println_out("Setting up wifi");
  strcpy(rtcData.ssid, "UPC1063844");
  strcpy(rtcData.psk, "GwiTsEjaftwdkXo");
  WiFi.mode(WIFI_STA);
  if( rtcData.valid ) {
	// The RTC data was good, make a quick connection
	println_out("RTC memory valid, connecting with know BSSID, channel etc..");
	println_out(rtcData.ssid);
	println_out(rtcData.psk);
	Serial.println(rtcData.channel);
	Serial.print(rtcData.bssid[0]);Serial.print(rtcData.bssid[1]);Serial.print(rtcData.bssid[2]);Serial.print(rtcData.bssid[3]);Serial.print(rtcData.bssid[4]);Serial.println(rtcData.bssid[5]);
	WiFi.begin( rtcData.ssid, rtcData.psk, rtcData.channel, rtcData.bssid, true );
  }
  else {
	// The RTC data was not valid, so make a regular connection
	WiFi.begin();
	rtcData.attempts = 0;
  }
}

void Wait_for_WiFi() {
	int retries = 0;
	int wifiStatus = WiFi.status();
	print_out("\nWaiting for wifi connection");
	while( wifiStatus != WL_CONNECTED ) {
		retries++;
		if( retries == 100 ) {
		// Quick connect is not working, reset WiFi and try regular connection
		print_out("\n10s gone, nothing happend. Resetting wifi settings\n");
		WiFi.disconnect();
		WiFi.begin(rtcData.ssid, rtcData.psk);
		}
		if( retries == 300 && reset_reason(0) == "POWERON_RESET" ) {  //Ha kapcsolóval kapcsolom be, ilyenkor az RTC resetelődik szóval harminc másodperc után mehet wifimanager oldal.
		// Giving up after 30 seconds and going back to sleep
		delay( 1 );
		start_wifimanager();
		if ( WiFi.status() != WL_CONNECTED ){
			WiFi.mode( WIFI_OFF );
			go_sleep(SLEEP_TIME_NO_WIFI, 0);
			return; // Not expecting this to be called, the previous call will never return.
		}
		}
		if( retries == 300 && rtcData.attempts >= 1 ) {                //Ha deepsleepből ébredt és ez a sokadik próbálkozás hogy nem talál wifi hálózatot.
		print_out("\n30s gone, nothing happend. Going to sleep. Number of wifi connection attempts wihout wifi connection: ");
		println_out(String(rtcData.attempts));
		// Giving up after 30 seconds and going back to sleep
		delay( 1 );
		if ( wifiStatus != WL_CONNECTED ){
			rtcData.attempts++;
			WiFi.mode( WIFI_OFF );
			go_sleep(SLEEP_TIME_NO_WIFI, 0);
			return; // Not expecting this to be called, the previous call will never return.
		}
		}
		if( retries == 900 && rtcData.attempts <= 1 ) {                //Ha deepsleepből ébredt és ez az első egynéhány próbálkozás a csatlakozásra.
		println_out("\n90s gone, nothing happend. Resetting wifi settings\n");
		// Giving up after 90 seconds and going back to sleep
		delay( 1 );
		if ( wifiStatus != WL_CONNECTED ){
			rtcData.attempts++;
			WiFi.mode( WIFI_OFF );
			go_sleep(SLEEP_TIME_NO_WIFI, 0);
			return; // Not expecting this to be called, the previous call will never return.
		}
		}
		delay( 100 );
		wifiStatus = WiFi.status();
		Serial.print(".");
	}
	Serial.print("\n");
	rtcData.attempts = 0;
	rtcData.channel = WiFi.channel();
	memcpy( rtcData.bssid, WiFi.BSSID(), 6 ); // Copy 6 bytes of BSSID (AP's MAC address)
	memcpy(rtcData.ssid, WiFi.SSID().c_str(), WiFi.SSID().length());
	memcpy(rtcData.psk, WiFi.psk().c_str(), WiFi.psk().length());
}

void start_wifimanager() {
  print_out("Turn on reason is \"Power on\" starting wifimanager config portal");
  AsyncWebServer server(80);
  DNSServer dns;
  char device_id_wm[16];
#if SZELEP
  String AP_name = "szelepvezerlo ";
#else
  String AP_name = "szenzor ";
#endif	
  String ID = String((uint32_t)(ESP.getEfuseMac() >> 32), HEX) + String((uint32_t)ESP.getEfuseMac(), HEX);
  ID.toCharArray(device_id_wm, 16);
  AsyncWiFiManager wifiManager(&server,&dns);
  AsyncWiFiManagerParameter print_device_ID(device_id, device_id, device_id_wm, 25);
  wifiManager.addParameter(&print_device_ID);
  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  wifiManager.startConfigPortal(AP_name.c_str());
  delay(1000);
  if ( WiFi.status() == WL_CONNECTED ){
	rtcData.attempts = 0;
	rtcData.channel = WiFi.channel();
	memcpy( rtcData.bssid, WiFi.BSSID(), 6 ); // Copy 6 bytes of BSSID (AP's MAC address)
	memcpy(rtcData.ssid, WiFi.SSID().c_str(), WiFi.SSID().length());
	memcpy(rtcData.psk, WiFi.psk().c_str(), WiFi.psk().length());
  }
}


