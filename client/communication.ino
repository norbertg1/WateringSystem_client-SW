
void mqtt_callback(char* topic, byte* payload, unsigned int length) {               //ezekre a csatornakra iratkozok fel
  char buff[70];
  Serial.println("MQTT callback");   
  Serial.println(topic);
  sprintf (buff, "%s%s", device_id, "/ON_OFF_COMMAND");
  if (!strcmp(topic, buff)) {
    on_off_command = payload[0] - 48;
    Serial.print("Valve command: ");  Serial.println(on_off_command);
    mqtt_done++;
  }
  sprintf (buff, "%s%s", device_id, "/DELAY_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    delay_time_seconds = atoi(buff);
    Serial.print("Delay time_seconds: "); Serial.println(delay_time_seconds);
    mqtt_done++;
  }
  sprintf (buff, "%s%s", device_id, "/SLEEP_TIME");
  if (!strcmp(topic, buff)) {
    for (int i = 0; i < length; i++) buff[i] = (char)payload[i];
    buff[length] = '\n';
    sleep_time_seconds = atoi(buff);
    Serial.print("Sleep time_seconds: "); Serial.println(sleep_time_seconds);
    mqtt_done++;
  }
  sprintf (buff, "%s%s", device_id, "/REMOTE_UPDATE");
  if (!strcmp(topic, buff)) {
    remote_update = payload[0] - 48;
    Serial.print("Remote update: "); Serial.println(remote_update);
    mqtt_done++;
  }
}

void mqtt_reconnect() {
  char buf_name[50];
  int i=0;
  int attempts = 3;
  if (valve_state()) attempts=20;
  while(client.state() != 0 && i < attempts){
    if(!client.connected()) {
      String clientId = "ESP8266Client-";
      clientId += String(ESP.getChipId(), HEX);
     
      client.connect(clientId.c_str(),"titok" , "titok");       //Az Ubuntun futó mosquitoba a bejelentkezési adatok
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
      mqttsend_i(i, device_id, "/DEBUG");
      client.loop();
    }
    if (i>1)  delay(1000);
    if (i>10) {           //nem tudom miert, talan bugos de ez kell ha nem akar csatlakozni
      espClient.setCertificate(certificates_esp8266_bin_crt, certificates_esp8266_bin_crt_len);
      espClient.setPrivateKey(certificates_esp8266_bin_key, certificates_esp8266_bin_key_len);
    }
    Serial.print("\nattempt = "); Serial.print(++i);
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

void mqttsend_s(const char *payload, char* device_id, char* topic){
  char buff_topic[50];
  sprintf (buff_topic, "%s%s", device_id, topic);
  client.publish(buff_topic, payload);
}

void setup_wifi() {

  Serial.println("Setting up wifi");
  char device_id_wm[25];
#if SZELEP
  String AP_name = "szelepvezerlo ";
#else
  String AP_name = "szenzor ";
#endif
  AP_name += String(ESP.getChipId(), HEX) + "-" + String(ESP.getFlashChipId(), HEX);
  String ID = String(ESP.getChipId(), HEX) + "-" + String(ESP.getFlashChipId(), HEX);
  ID.toCharArray(device_id_wm, 25);

  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;

  WiFiManagerParameter print_device_ID(device_id, device_id, device_id_wm, 25);
  wifiManager.addParameter(&print_device_ID);
  if( ESP.getResetReason() != "Power on") {             //A setApcallback  meghiv egy funkciot ami az Acess Point működése alatt fog lefutni. Én itt berakom az eszközt deepsleepbe (lambda funkcio, specialitas). De lehet a WIFI_CONFIGURATION_PAGE_TIMEOUT kéne nullára raknom majd kiprobalom.
    Serial.print("Turn on reason is not \"Power on\" (probably wake up from deepsleep). Therefor AP mode is unnecessary in case of not found know wifi. Entering deepsleep (again) for:"); Serial.print((int)SLEEP_TIME_NO_WIFI); Serial.println(" s");
    wifiManager.setAPCallback([](WiFiManager * wifi_manager) {go_sleep(SLEEP_TIME_NO_WIFI);}); //wifiManager.setAPCallback(go_sleep_callback); <---ezt igy is lehetne funkcioval
  }
  else wifiManager.setAPCallback(NULL); //probaljam meg kikomentezni
  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  //if(ESP.getResetReason() == "Power on")  wifiManager.setConfigPortalTimeout(WIFI_CONFIGURATION_PAGE_TIMEOUT);
  //else  wifiManager.setConfigPortalTimeout(0);
  wifiManager.setConnectTimeout(WIFI_CONNECTION_TIMEOUT);
  if (!wifiManager.autoConnect(AP_name.c_str())) {
    Serial.println("Failed to connect and hit timeout. Entering deep sleep!");
    //valve_turn_off();
    go_sleep(SLEEP_TIME_NO_WIFI);
  }
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

void web_update(long long minutes) {
  long long i = 0;
  minutes = minutes * 1000 * 3600;
  web_update_setup();
  while (1)     {
    server.handleClient();
    delay(1);
    i++;
    if (i >= minutes) {
      Serial.println("Timeout reached, restarting");
      f.close();
      ESP.restart();
    }
  }
}

void web_log(long long minutes){
  Serial.println("WEB LOG starting!!!");
  doFTP();  
}


//FTP stuff
const char* userName = "odroid";
const char* password = "odroid";

//File Operation


char outBuf[128];
char outCount;

WiFiClient dclient;
WiFiClient cclient;

byte doFTP()
{
    char fileName[13];
    sprintf(fileName, "%s%s", "/", device_id);
    File fh = SPIFFS.open(fileName, "r");
    if (!fh) {
      Serial.println("file open failed");
    }
  if (cclient.connect(FTP_SERVER,21)) {
    Serial.println(F("Command connected"));
  }
  else {
    fh.close();
    Serial.println(F("Command connection failed"));
    return 0;
  }

  if(!eRcv()) return 0;

  cclient.print("USER ");
  cclient.println(userName);
 
  if(!eRcv()) return 0;
 
  cclient.print("PASS ");
  cclient.println(password);
 
  if(!eRcv()) return 0;
 
  cclient.println("SYST");

  if(!eRcv()) return 0;

  cclient.println("Type I");

  if(!eRcv()) return 0;

  cclient.println("PASV");

  if(!eRcv()) return 0;

  char *tStr = strtok(outBuf,"(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL,"(,");
    array_pasv[i] = atoi(tStr);
    if(tStr == NULL)
    {
      Serial.println(F("Bad PASV Answer"));   

    }
  }

  unsigned int hiPort,loPort;
  hiPort=array_pasv[4]<<8;
  loPort=array_pasv[5]&255;
  Serial.print(F("Data port: "));
  hiPort = hiPort|loPort;
  Serial.println(hiPort);
  if(dclient.connect(FTP_SERVER, hiPort)){
    Serial.println("Data connected");
  }
  else{
    Serial.println("Data connection failed");
    cclient.stop();
    fh.close();
  }
 
  cclient.print("STOR ");
  cclient.println(fileName);
  if(!eRcv())
  {
    dclient.stop();
    return 0;
  }
  Serial.println(F("Writing"));
 
  byte clientBuf[64];
  int clientCount = 0;
 
  while(fh.available())
  {
    clientBuf[clientCount] = fh.read();
    clientCount++;
 
    if(clientCount > 63)
    {
      dclient.write((const uint8_t *)clientBuf, 64);
      clientCount = 0;
    }
  }
  if(clientCount > 0) dclient.write((const uint8_t *)clientBuf, clientCount);

  dclient.stop();
  Serial.println(F("Data disconnected"));
  cclient.println();
  if(!eRcv()) return 0;

  cclient.println("QUIT");

  if(!eRcv()) return 0;

  cclient.stop();
  Serial.println(F("Command disconnected"));

  fh.close();
  Serial.println(F("File closed"));
  return 1;
}

byte eRcv()
{
  byte respCode;
  byte thisByte;

  while(!cclient.available()) delay(1);

  respCode = cclient.peek();

  outCount = 0;

  while(cclient.available())
  { 
    thisByte = cclient.read();   
    Serial.write(thisByte);

    if(outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;     
      outBuf[outCount] = 0;
    }
  }

  if(respCode >= '4')
  {
    efail();
    return 0; 
  }

  return 1;
}


void efail()
{
  byte thisByte = 0;

  cclient.println(F("QUIT"));

  while(!cclient.available()) delay(1);

  while(cclient.available())
  { 
    thisByte = cclient.read();   
    Serial.write(thisByte);
  }

  cclient.stop();
  Serial.println(F("Command disconnected"));
}
