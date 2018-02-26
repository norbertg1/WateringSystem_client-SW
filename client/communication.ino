#include "communication.h"

void mqtt_callback(char* topic, byte* payload, unsigned int length) {               //ezekre a csatornakra iratkozok fel
  char buff[50];
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
  if(!client.connected()) {
    String clientId = "ESP8266Client-";
    clientId += String(ESP.getChipId(), HEX);
    client.connect(clientId.c_str(),device_name , device_passwd);
    //client.connect(clientId.c_str());
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
  Serial.print("The mqtt state is: "); Serial.println(client.state());
}

void setup_wifi() {

  Serial.println("Setting up wifi");
  char soil;
  EEPROM.begin(512);
  for(int i=0; i<127;i++) {
    device_id[i]=EEPROM.read(i);
    mqtt_password[i]=EEPROM.read(128+i)^EEPROM.read(127); //some cryptography
  }
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;

  WiFiManagerParameter custom_device_id("Device_ID", "Device ID", device_id, 255);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "mqtt password", mqtt_password, 255);
  wifiManager.addParameter(&custom_device_id);
  wifiManager.addParameter(&custom_mqtt_password);
  if( ESP.getResetReason() != "Power on") wifiManager.setAPCallback([](WiFiManager * wifi_manager) {go_sleep(SLEEP_TIME_NO_WIFI);}); //wifiManager.setAPCallback(go_sleep_callback);
  else wifiManager.setAPCallback(NULL); //probaljam meg kikomentezni
  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  wifiManager.setConnectTimeout(WIFI_CONNECTION_TIMEOUT);
  if (!wifiManager.autoConnect(device_name)) {
    Serial.println("Failed to connect and hit timeout. Entering deep sleep!");
    valve_turn_off();
    go_sleep(SLEEP_TIME_NO_WIFI);
  }
  strcpy(device_id, custom_device_id.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  randomSeed(ESP.getCycleCount());
  soil=random(0,255);
  EEPROM.write(127,soil);
  for(int i=0; i<127;i++) {
    EEPROM.write(i,device_id[i]);
    EEPROM.write(128+i,mqtt_password[i]^soil);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println(device_id);
  Serial.println(mqtt_password);
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

  Serial.printf("Ready for update through browser! Open http://%s in your browser\n", host);
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


