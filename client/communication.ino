
void mqtt_callback(char* topic, byte* payload, unsigned int length) {               //ezekre a csatornakra iratkozok fel
  char buff[70];
  Serial.println("MQTT callback");   
  Serial.println(topic);
  sprintf (buff, "%s%s", device_id, "/ON_OFF_COMMAND");
  if (!strcmp(topic, buff)) {
    on_off_command = payload[0] - 48;
    Serial.print("Valve command: ");  Serial.println(on_off_command);
  }
  sprintf (buff, "%s%s", device_id, "/DELAY_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    delay_time_seconds = atoi(buff);
    Serial.print("Delay time_seconds: "); Serial.println(delay_time_seconds);
  }
  sprintf (buff, "%s%s", device_id, "/SLEEP_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    sleep_time_seconds = atoi(buff);
    Serial.print("Sleep time_seconds: "); Serial.println(sleep_time_seconds);
  }
  sprintf (buff, "%s%s", device_id, "/REMOTE_UPDATE");
  if (!strcmp(topic, buff)) {
    remote_update = payload[0] - 48;
    Serial.print("Remote update: "); Serial.println(remote_update);
  }
}

void mqtt_reconnect() {
  char buf_name[50];
  int i=0;
  while(client.state() != 0 && i < 3){
    if(!client.connected()) {
      String clientId = "ESP8266Client-";
      clientId += String(ESP.getChipId(), HEX);
     
      client.connect(clientId.c_str(),"titok" , "titok");
      sprintf (buf_name, "%s%s", device_id, "/ON_OFF_COMMAND");
      client.subscribe(buf_name);
      client.loop();
      sprintf (buf_name, "%s%s", device_id, "/SLEEP_TIME");
      client.subscribe(buf_name);
      client.loop();
      sprintf (buf_name, "%s%s", device_id, "/DELAY_TIME");
      client.subscribe(buf_name);
      client.loop();
      sprintf (buf_name, "%s%s", device_id, "/REMOTE_UPDATE");
      client.subscribe(buf_name);
      client.loop();
    }
    Serial.print("\nattempt= "); Serial.print(++i);
    Serial.print(" The mqtt state is: "); Serial.println(client.state());
    
  }
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

void mqttsend_s(char *payload, char* device_id, char* topic){
  char buff_topic[50];
  sprintf (buff_topic, "%s%s", device_id, topic);
  client.publish(buff_topic, payload);
}

void setup_wifi() {

  Serial.println("Setting up wifi");
  char device_id_wm[25];
#if SZELEP
  String AP_name = "ESP8266 Locsolo-";
#else
  String AP_name = "ESP8266 szenzor-";
#endif
  AP_name += String(ESP.getChipId(), HEX);
  String ID = String(ESP.getChipId(), HEX) + "-" + String(ESP.getFlashChipId(), HEX);
  ID.toCharArray(device_id_wm, 25);

  
  EEPROM.begin(512);
  for(int i=0; i<127;i++) {
    device_name[i]=EEPROM.read(i);
  }
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;

  WiFiManagerParameter custom_device_name("Device_Name", "Device Name", device_name, 127);
  WiFiManagerParameter print_device_ID(device_id, device_id, device_id_wm, 25);
  wifiManager.addParameter(&custom_device_name);
  wifiManager.addParameter(&print_device_ID);
  if( ESP.getResetReason() != "Power on") {             //A setApcallback  meghiv egy funkciot ami az Acess Point működése alatt fog lefutni. Én itt berakom az eszközt deepsleepbe (lambda funkcio, specialitas). De lehet a WIFI_CONFIGURATION_PAGE_TIMEOUT kéne nullára raknom majd kiprobalom.
    Serial.print("Turn on reason is not \"Power on\" (probably wake up from deepsleep). Therefor AP mode is unnecessary in case of not found know wifi. Entering deepsleep (again) for:"); Serial.print(SLEEP_TIME_NO_WIFI); Serial.println(" s");
    wifiManager.setAPCallback([](WiFiManager * wifi_manager) {go_sleep(SLEEP_TIME_NO_WIFI);}); //wifiManager.setAPCallback(go_sleep_callback); <---ezt igy is lehetne funkcioval
  }
  else wifiManager.setAPCallback(NULL); //probaljam meg kikomentezni
  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  //if(ESP.getResetReason() == "Power on")  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  //else  wifiManager.setConfigPortalTimeout(0);
  wifiManager.setConnectTimeout(WIFI_CONNECTION_TIMEOUT);
  if (!wifiManager.autoConnect(AP_name.c_str())) {
    Serial.println("Failed to connect and hit timeout. Entering deep sleep!");
    valve_turn_off();
    go_sleep(SLEEP_TIME_NO_WIFI);
  }
  strcpy(device_name, custom_device_name.getValue());
  for(int i=0; i<127;i++) {
    EEPROM.write(i,device_name[i]);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println(device_name);
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

//  Serial.printf("Ready for update through browser! Open http://%s in your browser\n", host);
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


